//
//  MockDataService.swift
//  PQTR
//
//  Created by z on 1/9/2025.
//

import Foundation

class MockDataService: ObservableObject {
    static let shared = MockDataService()
    
    @Published var users: [User] = User.mockUsers
    @Published var organizations: [Organization] = Organization.mockOrganizations
    @Published var projects: [Project] = Project.mockProjects
    @Published var userOrganizations: [UserOrganization] = UserOrganization.mockUserOrganizations
    
    private init() {}
    
    // MARK: - Mock API Methods
    func getCurrentUser() async -> User {
        // Simulate network delay
        try? await Task.sleep(nanoseconds: 500_000_000) // 0.5 seconds
        return users.first ?? User.mockUsers[0]
    }
    
    func getUsers() async -> [User] {
        try? await Task.sleep(nanoseconds: 300_000_000) // 0.3 seconds
        return users
    }
    
    func getOrganizations() async -> [Organization] {
        try? await Task.sleep(nanoseconds: 400_000_000) // 0.4 seconds
        return organizations
    }
    
    func getProjects(for organizationId: UUID? = nil) async -> [Project] {
        try? await Task.sleep(nanoseconds: 350_000_000) // 0.35 seconds
        
        if let orgId = organizationId {
            return projects.filter { $0.organizationId == orgId }
        }
        return projects
    }
    
    func getUserOrganizations(for userId: UUID) async -> [UserOrganization] {
        try? await Task.sleep(nanoseconds: 250_000_000) // 0.25 seconds
        return userOrganizations.filter { $0.userId == userId }
    }
    
    func createProject(_ project: Project) async -> Project {
        try? await Task.sleep(nanoseconds: 600_000_000) // 0.6 seconds
        
        let newProject = Project(
            id: UUID(),
            name: project.name,
            description: project.description,
            organizationId: project.organizationId,
            isActive: project.isActive,
            createdAt: Date(),
            updatedAt: Date()
        )
        
        await MainActor.run {
            projects.append(newProject)
        }
        
        return newProject
    }
    
    // MARK: - Development Helpers
    func resetToMockData() {
        users = User.mockUsers
        organizations = Organization.mockOrganizations
        projects = Project.mockProjects
        userOrganizations = UserOrganization.mockUserOrganizations
    }
    
    func addMockProject(name: String, description: String, organizationId: UUID) {
        let newProject = Project(
            id: UUID(),
            name: name,
            description: description,
            organizationId: organizationId,
            isActive: true,
            createdAt: Date(),
            updatedAt: Date()
        )
        projects.append(newProject)
    }
}
