//
//  SignInView.swift
//  PQTR
//
//  Created by z on 1/9/2025.
//

import SwiftUI

struct SignInView: View {
    @EnvironmentObject var appViewModel: AppViewModel
    @State private var email = "demo@pqtr.ai"
    @State private var password = "demo123"
    @State private var isSigningIn = false
    @State private var errorMessage: String?
    @State private var showingCreateAccount = false
    
    var body: some View {
        NavigationView {
            VStack(spacing: 24) {
                // App Logo/Title
                VStack(spacing: 16) {
                    Image(systemName: "camera.fill")
                        .font(.system(size: 50))
                        .foregroundColor(.blue)
                    
                    Text("PQTR")
                        .font(.largeTitle)
                        .fontWeight(.bold)
                }
                .padding(.top, 60)
                
                Spacer()
                
                // Sign In Form
                VStack(spacing: 16) {
                    TextField("Email", text: $email)
                        .textFieldStyle(RoundedBorderTextFieldStyle())
                        .keyboardType(.emailAddress)
                        .autocapitalization(.none)
                        .autocorrectionDisabled()
                    
                    SecureField("Password", text: $password)
                        .textFieldStyle(RoundedBorderTextFieldStyle())
                    
                    if let errorMessage = errorMessage {
                        Text(errorMessage)
                            .font(.caption)
                            .foregroundColor(.red)
                    }
                    
                    Button(action: signIn) {
                        HStack {
                            if isSigningIn {
                                ProgressView()
                                    .scaleEffect(0.8)
                            }
                            Text(isSigningIn ? "Signing In..." : "Sign In")
                        }
                        .frame(maxWidth: .infinity)
                        .padding()
                        .background(Color.blue)
                        .foregroundColor(.white)
                        .cornerRadius(8)
                    }
                    .disabled(isSigningIn)
                    
                    Button("Create Account") {
                        showingCreateAccount = true
                    }
                    .foregroundColor(.blue)
                }
                .padding(.horizontal, 32)
                
                Spacer()
            }
            .navigationBarHidden(true)
            .sheet(isPresented: $showingCreateAccount) {
                CreateAccountView()
                    .environmentObject(appViewModel)
            }
        }
    }
    
    private func signIn() {
        errorMessage = nil
        isSigningIn = true
        
        Task {
            do {
                try await appViewModel.authService.signIn(email: email, password: password)
            } catch {
                await MainActor.run {
                    errorMessage = "Sign in failed. Please try again."
                    isSigningIn = false
                }
            }
        }
    }
}

#Preview {
    SignInView()
        .environmentObject(AppViewModel())
}
