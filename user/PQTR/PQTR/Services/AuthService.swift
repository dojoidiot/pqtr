//
//  AuthService.swift
//  PQTR
//
//  Created by z on 1/9/2025.
//

import Foundation
import Security

class AuthService: ObservableObject {
    static let shared = AuthService()
    
    @Published var isAuthenticated = false
    @Published var currentUser: User?
    
    private let keychain = KeychainService()
    private let authTokenKey = "pqtr_auth_token"
    
    private init() {
        checkAuthenticationStatus()
    }
    
    // MARK: - Authentication Status
    private func checkAuthenticationStatus() {
        if let token = keychain.get(authTokenKey) {
            authToken = token
            isAuthenticated = true
            loadCurrentUser()
        }
    }
    
    // MARK: - Token Management
    var authToken: String? {
        get { keychain.get(authTokenKey) }
        set {
            if let token = newValue {
                keychain.set(token, forKey: authTokenKey)
            } else {
                keychain.delete(authTokenKey)
            }
        }
    }
    
    // MARK: - Authentication Methods
    func signIn(email: String, password: String) async throws {
        // Mock authentication - in real app, call your auth endpoint
        let mockToken = "mock_jwt_token_\(UUID().uuidString)"
        
        // Simulate network delay
        try await Task.sleep(nanoseconds: 1_000_000_000) // 1 second
        
        authToken = mockToken
        isAuthenticated = true
        
        // Load user data asynchronously
        await loadCurrentUserAsync()
    }
    
    func signOut() {
        authToken = nil
        isAuthenticated = false
        currentUser = nil
    }
    
    // MARK: - User Management
    private func loadCurrentUser() {
        currentUser = User.mockUsers[0]
    }
    
    @MainActor
    private func loadCurrentUserAsync() async {
        do {
            currentUser = try await NetworkService.shared.getCurrentUser()
        } catch {
            // Fallback to mock user
            currentUser = User.mockUsers[0]
        }
    }
}

// MARK: - Keychain Service
class KeychainService {
    private let service = "co.horyzon.pqtr-ios"
    
    func set(_ value: String, forKey key: String) {
        let data = value.data(using: .utf8)!
        
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrAccount as String: key,
            kSecValueData as String: data
        ]
        
        // Delete existing item
        SecItemDelete(query as CFDictionary)
        
        // Add new item
        SecItemAdd(query as CFDictionary, nil)
    }
    
    func get(_ key: String) -> String? {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrAccount as String: key,
            kSecReturnData as String: true,
            kSecMatchLimit as String: kSecMatchLimitOne
        ]
        
        var result: AnyObject?
        let status = SecItemCopyMatching(query as CFDictionary, &result)
        
        guard status == errSecSuccess,
              let data = result as? Data,
              let string = String(data: data, encoding: .utf8) else {
            return nil
        }
        
        return string
    }
    
    func delete(_ key: String) {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrAccount as String: key
        ]
        
        SecItemDelete(query as CFDictionary)
    }
}
