//
//  CreateAccountView.swift
//  PQTR
//
//  Created by z on 1/9/2025.
//

import SwiftUI

struct CreateAccountView: View {
    @EnvironmentObject var appViewModel: AppViewModel
    @Environment(\.dismiss) private var dismiss
    
    @State private var displayName = ""
    @State private var email = ""
    @State private var password = ""
    @State private var confirmPassword = ""
    @State private var isCreating = false
    @State private var errorMessage: String?
    
    var body: some View {
        NavigationView {
            ScrollView {
                VStack(spacing: 24) {
                    // Header
                    VStack(spacing: 16) {
                        Text("Create Account")
                            .font(.largeTitle)
                            .fontWeight(.bold)
                    }
                    .padding(.top, 40)
                    
                    // Form
                    VStack(spacing: 16) {
                        TextField("Display Name", text: $displayName)
                            .textFieldStyle(RoundedBorderTextFieldStyle())
                            .autocapitalization(.words)
                            .autocorrectionDisabled()
                        
                        TextField("Email", text: $email)
                            .textFieldStyle(RoundedBorderTextFieldStyle())
                            .keyboardType(.emailAddress)
                            .autocapitalization(.none)
                            .autocorrectionDisabled()
                        
                        SecureField("Password", text: $password)
                            .textFieldStyle(RoundedBorderTextFieldStyle())
                        
                        SecureField("Confirm Password", text: $confirmPassword)
                            .textFieldStyle(RoundedBorderTextFieldStyle())
                        
                        if let errorMessage = errorMessage {
                            Text(errorMessage)
                                .font(.caption)
                                .foregroundColor(.red)
                        }
                        
                        Button(action: createAccount) {
                            HStack {
                                if isCreating {
                                    ProgressView()
                                        .scaleEffect(0.8)
                                }
                                Text(isCreating ? "Creating..." : "Create Account")
                            }
                            .frame(maxWidth: .infinity)
                            .padding()
                            .background(Color.blue)
                            .foregroundColor(.white)
                            .cornerRadius(8)
                        }
                        .disabled(isCreating)
                    }
                    .padding(.horizontal, 32)
                    
                    Spacer(minLength: 20)
                }
            }
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .navigationBarLeading) {
                    Button("Cancel") {
                        dismiss()
                    }
                }
            }
        }
    }
    
    private func createAccount() {
        guard !displayName.isEmpty && !email.isEmpty && !password.isEmpty && password == confirmPassword else {
            errorMessage = "Please fill in all fields and ensure passwords match."
            return
        }
        
        errorMessage = nil
        isCreating = true
        
        Task {
            try? await Task.sleep(nanoseconds: 1_500_000_000) // 1.5 second delay
            
            await MainActor.run {
                isCreating = false
                dismiss()
            }
        }
    }
}

#Preview {
    CreateAccountView()
        .environmentObject(AppViewModel())
}
