//
//  ContentView.swift
//  PQTR
//
//  Created by z on 1/9/2025.
//

import SwiftUI

struct ContentView: View {
    @StateObject private var appViewModel = AppViewModel()
    
    var body: some View {
        Group {
            if appViewModel.isAuthenticated {
                MainTabView()
                    .environmentObject(appViewModel)
            } else {
                SignInView()
                    .environmentObject(appViewModel)
            }
        }
        .animation(.easeInOut, value: appViewModel.isAuthenticated)
    }
}

struct MainTabView: View {
    @EnvironmentObject var appViewModel: AppViewModel
    
    var body: some View {
        TabView(selection: $appViewModel.selectedTab) {
            HomeView()
                .tabItem {
                    Image(systemName: Tab.home.rawValue)
                    Text(Tab.home.title)
                }
                .tag(Tab.home)
            
            ProjectsView()
                .tabItem {
                    Image(systemName: Tab.projects.rawValue)
                    Text(Tab.projects.title)
                }
                .tag(Tab.projects)
            
            OrganizationsView()
                .tabItem {
                    Image(systemName: Tab.organizations.rawValue)
                    Text(Tab.organizations.title)
                }
                .tag(Tab.organizations)
            
            ProfileView()
                .tabItem {
                    Image(systemName: Tab.profile.rawValue)
                    Text(Tab.profile.title)
                }
                .tag(Tab.profile)
        }
        .accentColor(.blue)
    }
}

#Preview {
    ContentView()
}
