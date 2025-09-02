//
//  HomeView.swift
//  PQTR
//
//  Created by z on 1/9/2025.
//

import SwiftUI

struct HomeView: View {
    @EnvironmentObject var appViewModel: AppViewModel
    
    var body: some View {
        NavigationView {
            ScrollView {
                VStack(alignment: .leading, spacing: 20) {
                    // Welcome Section
                    welcomeSection
                    
                    // Quick Stats
                    quickStatsSection
                    
                    // Recent Projects
                    recentProjectsSection
                    
                    // Organizations
                    organizationsSection
                }
                .padding()
            }
            .navigationTitle("PQTR")
            .refreshable {
                await appViewModel.loadData()
            }
        }
    }
    
    private var welcomeSection: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("Welcome back!")
                .font(.title2)
                .fontWeight(.semibold)
            
            if let user = appViewModel.currentUser {
                Text("Hello, \(user.firstName ?? "there")")
                    .font(.title3)
                    .foregroundColor(.secondary)
            }
        }
    }
    
    private var quickStatsSection: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("Quick Stats")
                .font(.headline)
                .fontWeight(.semibold)
            
            HStack(spacing: 16) {
                StatCard(
                    title: "Projects",
                    value: "\(appViewModel.projects.count)",
                    icon: "folder.fill",
                    color: .blue
                )
                
                StatCard(
                    title: "Organizations",
                    value: "\(appViewModel.organizations.count)",
                    icon: "building.2.fill",
                    color: .green
                )
                
                StatCard(
                    title: "Active",
                    value: "\(appViewModel.projects.filter { $0.isActive }.count)",
                    icon: "checkmark.circle.fill",
                    color: .orange
                )
            }
        }
    }
    
    private var recentProjectsSection: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                Text("Recent Projects")
                    .font(.headline)
                    .fontWeight(.semibold)
                
                Spacer()
                
                NavigationLink("View All", destination: ProjectsView())
                    .font(.caption)
                    .foregroundColor(.blue)
            }
            
            if appViewModel.projects.isEmpty {
                EmptyStateView(
                    icon: "folder",
                    title: "No Projects",
                    message: "Create your first project to get started"
                )
            } else {
                LazyVStack(spacing: 8) {
                    ForEach(Array(appViewModel.projects.prefix(3))) { project in
                        ProjectRowView(project: project)
                    }
                }
            }
        }
    }
    
    private var organizationsSection: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                Text("Organizations")
                    .font(.headline)
                    .fontWeight(.semibold)
                
                Spacer()
                
                NavigationLink("View All", destination: OrganizationsView())
                    .font(.caption)
                    .foregroundColor(.blue)
            }
            
            if appViewModel.organizations.isEmpty {
                EmptyStateView(
                    icon: "building.2",
                    title: "No Organizations",
                    message: "Join an organization to collaborate"
                )
            } else {
                LazyVStack(spacing: 8) {
                    ForEach(Array(appViewModel.organizations.prefix(2))) { organization in
                        OrganizationRowView(organization: organization)
                    }
                }
            }
        }
    }
}

// MARK: - Supporting Views
struct StatCard: View {
    let title: String
    let value: String
    let icon: String
    let color: Color
    
    var body: some View {
        VStack(spacing: 8) {
            Image(systemName: icon)
                .font(.title2)
                .foregroundColor(color)
            
            Text(value)
                .font(.title2)
                .fontWeight(.bold)
            
            Text(title)
                .font(.caption)
                .foregroundColor(.secondary)
        }
        .frame(maxWidth: .infinity)
        .padding()
        .background(Color(.systemGray6))
        .cornerRadius(12)
    }
}

struct ProjectRowView: View {
    let project: Project
    @EnvironmentObject var appViewModel: AppViewModel
    
    var body: some View {
        HStack(spacing: 12) {
            Image(systemName: "folder.fill")
                .font(.title2)
                .foregroundColor(.blue)
                .frame(width: 24)
            
            VStack(alignment: .leading, spacing: 4) {
                Text(project.name)
                    .font(.subheadline)
                    .fontWeight(.medium)
                
                if let description = project.description {
                    Text(description)
                        .font(.caption)
                        .foregroundColor(.secondary)
                        .lineLimit(2)
                }
                
                if let organization = appViewModel.getOrganization(for: project) {
                    Text(organization.name)
                        .font(.caption2)
                        .foregroundColor(.blue)
                        .padding(.horizontal, 8)
                        .padding(.vertical, 2)
                        .background(Color.blue.opacity(0.1))
                        .cornerRadius(4)
                }
            }
            
            Spacer()
            
            Image(systemName: "chevron.right")
                .font(.caption)
                .foregroundColor(.secondary)
        }
        .padding()
        .background(Color(.systemGray6))
        .cornerRadius(8)
    }
}

struct OrganizationRowView: View {
    let organization: Organization
    
    var body: some View {
        HStack(spacing: 12) {
            Image(systemName: "building.2.fill")
                .font(.title2)
                .foregroundColor(.green)
                .frame(width: 24)
            
            VStack(alignment: .leading, spacing: 4) {
                Text(organization.name)
                    .font(.subheadline)
                    .fontWeight(.medium)
                
                if let description = organization.description {
                    Text(description)
                        .font(.caption)
                        .foregroundColor(.secondary)
                        .lineLimit(2)
                }
            }
            
            Spacer()
            
            Image(systemName: "chevron.right")
                .font(.caption)
                .foregroundColor(.secondary)
        }
        .padding()
        .background(Color(.systemGray6))
        .cornerRadius(8)
    }
}

struct EmptyStateView: View {
    let icon: String
    let title: String
    let message: String
    
    var body: some View {
        VStack(spacing: 12) {
            Image(systemName: icon)
                .font(.largeTitle)
                .foregroundColor(.secondary)
            
            Text(title)
                .font(.headline)
                .fontWeight(.medium)
            
            Text(message)
                .font(.caption)
                .foregroundColor(.secondary)
                .multilineTextAlignment(.center)
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, 32)
    }
}

#Preview {
    HomeView()
        .environmentObject(AppViewModel())
}
