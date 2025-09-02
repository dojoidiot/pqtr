//
//  ProfileView.swift
//  PQTR
//
//  Created by z on 1/9/2025.
//

import SwiftUI

struct ProfileView: View {
    @EnvironmentObject var appViewModel: AppViewModel
    @State private var showingSignOutAlert = false
    
    var body: some View {
        NavigationView {
            List {
                // User Info Section
                if let user = appViewModel.currentUser {
                    userInfoSection(user)
                }
                
                // Statistics Section
                statisticsSection
                
                // Settings Section
                settingsSection
                
                // Sign Out Section
                signOutSection
            }
            .navigationTitle("Profile")
            .alert("Sign Out", isPresented: $showingSignOutAlert) {
                Button("Cancel", role: .cancel) { }
                Button("Sign Out", role: .destructive) {
                    appViewModel.signOut()
                }
            } message: {
                Text("Are you sure you want to sign out?")
            }
        }
    }
    
    private func userInfoSection(_ user: User) -> some View {
        Section {
            HStack(spacing: 16) {
                // Profile Picture Placeholder
                Circle()
                    .fill(Color.blue.gradient)
                    .frame(width: 60, height: 60)
                    .overlay {
                        Text(user.fullName.prefix(1).uppercased())
                            .font(.title2)
                            .fontWeight(.semibold)
                            .foregroundColor(.white)
                    }
                
                VStack(alignment: .leading, spacing: 4) {
                    Text(user.fullName.isEmpty ? "User" : user.fullName)
                        .font(.headline)
                        .fontWeight(.semibold)
                    
                    Text(user.email)
                        .font(.subheadline)
                        .foregroundColor(.secondary)
                    
                    HStack {
                        Circle()
                            .fill(user.isActive ? Color.green : Color.orange)
                            .frame(width: 8, height: 8)
                        
                        Text(user.isActive ? "Active" : "Inactive")
                            .font(.caption)
                            .foregroundColor(.secondary)
                    }
                }
                
                Spacer()
            }
            .padding(.vertical, 8)
        }
    }
    
    private var statisticsSection: some View {
        Section("Statistics") {
            HStack {
                Label("Projects", systemImage: "folder")
                Spacer()
                Text("\(appViewModel.projects.count)")
                    .fontWeight(.medium)
            }
            
            HStack {
                Label("Organizations", systemImage: "building.2")
                Spacer()
                Text("\(appViewModel.organizations.count)")
                    .fontWeight(.medium)
            }
            
            HStack {
                Label("Active Projects", systemImage: "checkmark.circle")
                Spacer()
                Text("\(appViewModel.projects.filter { $0.isActive }.count)")
                    .fontWeight(.medium)
            }
        }
    }
    
    private var settingsSection: some View {
        Section("Settings") {
            NavigationLink(destination: SettingsView()) {
                Label("Preferences", systemImage: "gearshape")
            }
            
            NavigationLink(destination: AboutView()) {
                Label("About", systemImage: "info.circle")
            }
            
            Button(action: {
                appViewModel.refreshData()
            }) {
                Label("Refresh Data", systemImage: "arrow.clockwise")
            }
            .foregroundColor(.primary)
        }
    }
    
    private var signOutSection: some View {
        Section {
            Button(action: {
                showingSignOutAlert = true
            }) {
                Label("Sign Out", systemImage: "rectangle.portrait.and.arrow.right")
                    .foregroundColor(.red)
            }
        }
    }
}

struct SettingsView: View {
    @AppStorage("useMockData") private var useMockData = true
    @AppStorage("enableNotifications") private var enableNotifications = true
    @AppStorage("autoRefresh") private var autoRefresh = true
    
    var body: some View {
        List {
            Section("Development") {
                Toggle("Use Mock Data", isOn: $useMockData)
                
                Toggle("Auto Refresh", isOn: $autoRefresh)
            }
            
            Section("Notifications") {
                Toggle("Enable Notifications", isOn: $enableNotifications)
            }
            
            Section("Data") {
                Button("Reset to Mock Data") {
                    // This would reset the mock data
                }
                .foregroundColor(.orange)
                
                Button("Clear Cache") {
                    // This would clear any cached data
                }
                .foregroundColor(.blue)
            }
        }
        .navigationTitle("Settings")
        .navigationBarTitleDisplayMode(.inline)
    }
}

struct AboutView: View {
    var body: some View {
        List {
            Section {
                VStack(alignment: .leading, spacing: 12) {
                    HStack {
                        Image(systemName: "camera.fill")
                            .font(.title)
                            .foregroundColor(.blue)
                        
                        VStack(alignment: .leading) {
                            Text("PQTR")
                                .font(.title2)
                                .fontWeight(.bold)
                            
                            Text("Professional Photography Tool")
                                .font(.subheadline)
                                .foregroundColor(.secondary)
                        }
                    }
                    
                    Text("PQTR is a professional photography management tool designed to help photographers organize their projects, collaborate with teams, and manage their workflow efficiently.")
                        .font(.subheadline)
                        .foregroundColor(.secondary)
                }
                .padding(.vertical, 8)
            }
            
            Section("Version") {
                HStack {
                    Text("Version")
                    Spacer()
                    Text("1.0.0")
                        .foregroundColor(.secondary)
                }
                
                HStack {
                    Text("Build")
                    Spacer()
                    Text("1")
                        .foregroundColor(.secondary)
                }
            }
            
            Section("Support") {
                Link("Help & Support", destination: URL(string: "https://pqtr.ai/help")!)
                Link("Privacy Policy", destination: URL(string: "https://pqtr.ai/privacy")!)
                Link("Terms of Service", destination: URL(string: "https://pqtr.ai/terms")!)
            }
        }
        .navigationTitle("About")
        .navigationBarTitleDisplayMode(.inline)
    }
}

#Preview {
    ProfileView()
        .environmentObject(AppViewModel())
}
