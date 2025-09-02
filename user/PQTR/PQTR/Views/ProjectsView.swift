//
//  ProjectsView.swift
//  PQTR
//
//  Created by z on 1/9/2025.
//

import SwiftUI

struct ProjectsView: View {
    @EnvironmentObject var appViewModel: AppViewModel
    @State private var showingCreateProject = false
    
    var body: some View {
        NavigationView {
            Group {
                if appViewModel.isLoading {
                    ProgressView("Loading projects...")
                        .frame(maxWidth: .infinity, maxHeight: .infinity)
                } else if appViewModel.projects.isEmpty {
                    EmptyProjectsView()
                } else {
                    projectsList
                }
            }
            .navigationTitle("Projects")
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button("New") {
                        showingCreateProject = true
                    }
                }
            }
            .refreshable {
                await appViewModel.loadData()
            }
            .sheet(isPresented: $showingCreateProject) {
                CreateProjectView()
            }
        }
    }
    
    private var projectsList: some View {
        List {
            ForEach(appViewModel.projects) { project in
                NavigationLink(destination: ProjectDetailView(project: project)) {
                    ProjectListRowView(project: project)
                }
            }
        }
        .listStyle(PlainListStyle())
    }
}

struct ProjectListRowView: View {
    let project: Project
    @EnvironmentObject var appViewModel: AppViewModel
    
    var body: some View {
        HStack(spacing: 12) {
            // Project Icon
            Image(systemName: project.isActive ? "folder.fill" : "folder")
                .font(.title2)
                .foregroundColor(project.isActive ? .blue : .secondary)
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
                
                HStack {
                    if let organization = appViewModel.getOrganization(for: project) {
                        Text(organization.name)
                            .font(.caption2)
                            .foregroundColor(.blue)
                            .padding(.horizontal, 6)
                            .padding(.vertical, 2)
                            .background(Color.blue.opacity(0.1))
                            .cornerRadius(4)
                    }
                    
                    Spacer()
                    
                    Text(project.createdAt, style: .relative)
                        .font(.caption2)
                        .foregroundColor(.secondary)
                }
            }
            
            Spacer()
            
            if !project.isActive {
                Image(systemName: "pause.circle.fill")
                    .foregroundColor(.orange)
            }
        }
        .padding(.vertical, 4)
    }
}

struct EmptyProjectsView: View {
    var body: some View {
        VStack(spacing: 20) {
            Image(systemName: "folder")
                .font(.system(size: 60))
                .foregroundColor(.secondary)
            
            VStack(spacing: 8) {
                Text("No Projects Yet")
                    .font(.title2)
                    .fontWeight(.semibold)
                
                Text("Create your first project to get started with PQTR")
                    .font(.subheadline)
                    .foregroundColor(.secondary)
                    .multilineTextAlignment(.center)
            }
            
            Button("Create Project") {
                // This will be handled by the parent view
            }
            .buttonStyle(.borderedProminent)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .padding()
    }
}

struct ProjectDetailView: View {
    let project: Project
    @EnvironmentObject var appViewModel: AppViewModel
    
    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 20) {
                // Project Header
                VStack(alignment: .leading, spacing: 8) {
                    Text(project.name)
                        .font(.title)
                        .fontWeight(.bold)
                    
                    if let description = project.description {
                        Text(description)
                            .font(.subheadline)
                            .foregroundColor(.secondary)
                    }
                    
                    HStack {
                        if let organization = appViewModel.getOrganization(for: project) {
                            Label(organization.name, systemImage: "building.2")
                                .font(.caption)
                                .foregroundColor(.blue)
                        }
                        
                        Spacer()
                        
                        Label(project.isActive ? "Active" : "Inactive", 
                              systemImage: project.isActive ? "checkmark.circle.fill" : "pause.circle.fill")
                            .font(.caption)
                            .foregroundColor(project.isActive ? .green : .orange)
                    }
                }
                
                Divider()
                
                // Project Info
                VStack(alignment: .leading, spacing: 12) {
                    Text("Project Details")
                        .font(.headline)
                        .fontWeight(.semibold)
                    
                    InfoRow(label: "Created", value: project.createdAt.formatted(date: .abbreviated, time: .omitted))
                    InfoRow(label: "Last Updated", value: project.updatedAt.formatted(date: .abbreviated, time: .omitted))
                    InfoRow(label: "Status", value: project.isActive ? "Active" : "Inactive")
                }
                
                Spacer()
            }
            .padding()
        }
        .navigationTitle("Project")
        .navigationBarTitleDisplayMode(.inline)
    }
}

struct InfoRow: View {
    let label: String
    let value: String
    
    var body: some View {
        HStack {
            Text(label)
                .font(.subheadline)
                .foregroundColor(.secondary)
            
            Spacer()
            
            Text(value)
                .font(.subheadline)
                .fontWeight(.medium)
        }
    }
}

#Preview {
    ProjectsView()
        .environmentObject(AppViewModel())
}
