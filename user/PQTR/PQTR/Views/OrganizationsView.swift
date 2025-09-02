//
//  OrganizationsView.swift
//  PQTR
//
//  Created by z on 1/9/2025.
//

import SwiftUI

struct OrganizationsView: View {
    @EnvironmentObject var appViewModel: AppViewModel
    
    var body: some View {
        NavigationView {
            Group {
                if appViewModel.isLoading {
                    ProgressView("Loading organizations...")
                        .frame(maxWidth: .infinity, maxHeight: .infinity)
                } else if appViewModel.organizations.isEmpty {
                    EmptyOrganizationsView()
                } else {
                    organizationsList
                }
            }
            .navigationTitle("Organizations")
            .refreshable {
                await appViewModel.loadData()
            }
        }
    }
    
    private var organizationsList: some View {
        List {
            ForEach(appViewModel.organizations) { organization in
                NavigationLink(destination: OrganizationDetailView(organization: organization)) {
                    OrganizationListRowView(organization: organization)
                }
            }
        }
        .listStyle(PlainListStyle())
    }
}

struct OrganizationListRowView: View {
    let organization: Organization
    @EnvironmentObject var appViewModel: AppViewModel
    
    var body: some View {
        HStack(spacing: 12) {
            // Organization Icon
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
                
                HStack {
                    Text("@\(organization.slug)")
                        .font(.caption2)
                        .foregroundColor(.blue)
                        .padding(.horizontal, 6)
                        .padding(.vertical, 2)
                        .background(Color.blue.opacity(0.1))
                        .cornerRadius(4)
                    
                    Spacer()
                    
                    if let role = appViewModel.getUserRole(in: organization) {
                        Text(role.displayName)
                            .font(.caption2)
                            .foregroundColor(.white)
                            .padding(.horizontal, 6)
                            .padding(.vertical, 2)
                            .background(roleColor(role))
                            .cornerRadius(4)
                    }
                }
            }
            
            Spacer()
            
            if !organization.isActive {
                Image(systemName: "pause.circle.fill")
                    .foregroundColor(.orange)
            }
        }
        .padding(.vertical, 4)
    }
    
    private func roleColor(_ role: Role) -> Color {
        switch role {
        case .admin: return .red
        case .member: return .blue
        case .viewer: return .gray
        }
    }
}

struct EmptyOrganizationsView: View {
    var body: some View {
        VStack(spacing: 20) {
            Image(systemName: "building.2")
                .font(.system(size: 60))
                .foregroundColor(.secondary)
            
            VStack(spacing: 8) {
                Text("No Organizations")
                    .font(.title2)
                    .fontWeight(.semibold)
                
                Text("You're not a member of any organizations yet")
                    .font(.subheadline)
                    .foregroundColor(.secondary)
                    .multilineTextAlignment(.center)
            }
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .padding()
    }
}

struct OrganizationDetailView: View {
    let organization: Organization
    @EnvironmentObject var appViewModel: AppViewModel
    
    private var organizationProjects: [Project] {
        appViewModel.projects.filter { $0.organizationId == organization.id }
    }
    
    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 20) {
                // Organization Header
                VStack(alignment: .leading, spacing: 8) {
                    Text(organization.name)
                        .font(.title)
                        .fontWeight(.bold)
                    
                    if let description = organization.description {
                        Text(description)
                            .font(.subheadline)
                            .foregroundColor(.secondary)
                    }
                    
                    HStack {
                        Text("@\(organization.slug)")
                            .font(.caption)
                            .foregroundColor(.blue)
                            .padding(.horizontal, 8)
                            .padding(.vertical, 4)
                            .background(Color.blue.opacity(0.1))
                            .cornerRadius(6)
                        
                        Spacer()
                        
                        if let role = appViewModel.getUserRole(in: organization) {
                            Text(role.displayName)
                                .font(.caption)
                                .foregroundColor(.white)
                                .padding(.horizontal, 8)
                                .padding(.vertical, 4)
                                .background(roleColor(role))
                                .cornerRadius(6)
                        }
                        
                        Label(organization.isActive ? "Active" : "Inactive", 
                              systemImage: organization.isActive ? "checkmark.circle.fill" : "pause.circle.fill")
                            .font(.caption)
                            .foregroundColor(organization.isActive ? .green : .orange)
                    }
                }
                
                Divider()
                
                // Projects Section
                VStack(alignment: .leading, spacing: 12) {
                    Text("Projects (\(organizationProjects.count))")
                        .font(.headline)
                        .fontWeight(.semibold)
                    
                    if organizationProjects.isEmpty {
                        EmptyStateView(
                            icon: "folder",
                            title: "No Projects",
                            message: "This organization doesn't have any projects yet"
                        )
                    } else {
                        LazyVStack(spacing: 8) {
                            ForEach(organizationProjects) { project in
                                NavigationLink(destination: ProjectDetailView(project: project)) {
                                    ProjectRowView(project: project)
                                }
                                .buttonStyle(PlainButtonStyle())
                            }
                        }
                    }
                }
                
                Divider()
                
                // Organization Info
                VStack(alignment: .leading, spacing: 12) {
                    Text("Organization Details")
                        .font(.headline)
                        .fontWeight(.semibold)
                    
                    InfoRow(label: "Created", value: organization.createdAt.formatted(date: .abbreviated, time: .omitted))
                    InfoRow(label: "Last Updated", value: organization.updatedAt.formatted(date: .abbreviated, time: .omitted))
                    InfoRow(label: "Status", value: organization.isActive ? "Active" : "Inactive")
                }
                
                Spacer()
            }
            .padding()
        }
        .navigationTitle("Organization")
        .navigationBarTitleDisplayMode(.inline)
    }
    
    private func roleColor(_ role: Role) -> Color {
        switch role {
        case .admin: return .red
        case .member: return .blue
        case .viewer: return .gray
        }
    }
}

#Preview {
    OrganizationsView()
        .environmentObject(AppViewModel())
}
