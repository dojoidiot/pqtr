//
//  AppViewModel.swift
//  PQTR
//
//  Created by z on 1/9/2025.
//

import Foundation
import SwiftUI
import Combine

@MainActor
class AppViewModel: ObservableObject {
    @Published var selectedTab: Tab = .home
    @Published var isLoading = false
    @Published var errorMessage: String?
    
    // Services
    let authService = AuthService.shared
    let networkService = NetworkService.shared
    let mockDataService = MockDataService.shared
    
    // Combine
    private var cancellables = Set<AnyCancellable>()
    
    // Data
    @Published var currentUser: User?
    @Published var organizations: [Organization] = []
    @Published var projects: [Project] = []
    
    // Authentication state
    var isAuthenticated: Bool {
        authService.isAuthenticated
    }
    @Published var userOrganizations: [UserOrganization] = []
    
    // Use mock data for development
    private let useMockData = true
    
    init() {
        setupBindings()
        loadInitialData()
    }
    
    private func setupBindings() {
        // Listen to auth service changes
        authService.$currentUser
            .sink { [weak self] user in
                self?.currentUser = user
            }
            .store(in: &cancellables)
    }
    
    private func loadInitialData() {
        Task {
            await loadData()
        }
    }
    
    func loadData() async {
        isLoading = true
        errorMessage = nil
        
        do {
            if useMockData {
                await loadMockData()
            } else {
                try await loadRealData()
            }
        } catch {
            errorMessage = error.localizedDescription
        }
        
        isLoading = false
    }
    
    private func loadMockData() async {
        currentUser = await mockDataService.getCurrentUser()
        organizations = await mockDataService.getOrganizations()
        projects = await mockDataService.getProjects()
        
        if let userId = currentUser?.id {
            userOrganizations = await mockDataService.getUserOrganizations(for: userId)
        }
    }
    
    private func loadRealData() async throws {
        currentUser = try await networkService.getCurrentUser()
        organizations = try await networkService.getOrganizations()
        projects = try await networkService.getProjects()
        
        if let userId = currentUser?.id {
            userOrganizations = try await networkService.getUserOrganizations(userId: userId)
        }
    }
    
    func refreshData() {
        Task {
            await loadData()
        }
    }
    
    func signOut() {
        authService.signOut()
        selectedTab = .home
        clearData()
    }
    
    private func clearData() {
        currentUser = nil
        organizations = []
        projects = []
        userOrganizations = []
    }
    
    // MARK: - Project Management
    func createProject(name: String, description: String?, organizationId: UUID) async {
        let newProject = Project(
            id: UUID(),
            name: name,
            description: description,
            organizationId: organizationId,
            isActive: true,
            createdAt: Date(),
            updatedAt: Date()
        )
        
        do {
            if useMockData {
                let createdProject = await mockDataService.createProject(newProject)
                projects.append(createdProject)
            } else {
                let createdProject = try await networkService.createProject(newProject)
                projects.append(createdProject)
            }
        } catch {
            errorMessage = "Failed to create project: \(error.localizedDescription)"
        }
    }
    
    // MARK: - Organization Helpers
    func getOrganization(for project: Project) -> Organization? {
        return organizations.first { $0.id == project.organizationId }
    }
    
    func getUserRole(in organization: Organization) -> Role? {
        guard let userId = currentUser?.id else { return nil }
        return userOrganizations.first { 
            $0.userId == userId && $0.organizationId == organization.id 
        }?.role
    }
}

// MARK: - Tab Enum
enum Tab: String, CaseIterable {
    case home = "house"
    case projects = "folder"
    case organizations = "building.2"
    case profile = "person"
    
    var title: String {
        switch self {
        case .home: return "Home"
        case .projects: return "Projects"
        case .organizations: return "Organizations"
        case .profile: return "Profile"
        }
    }
}
