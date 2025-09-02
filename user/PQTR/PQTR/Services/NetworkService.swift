//
//  NetworkService.swift
//  PQTR
//
//  Created by z on 1/9/2025.
//

import Foundation

class NetworkService: ObservableObject {
    static let shared = NetworkService()
    
    private let baseURL = "http://localhost:3000"
    private let session = URLSession.shared
    
    private init() {}
    
    // MARK: - Generic Request Method
    private func request<T: Codable>(
        endpoint: String,
        method: HTTPMethod = .GET,
        body: Data? = nil,
        responseType: T.Type
    ) async throws -> T {
        guard let url = URL(string: "\(baseURL)\(endpoint)") else {
            throw NetworkError.invalidURL
        }
        
        var request = URLRequest(url: url)
        request.httpMethod = method.rawValue
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        
        // Add JWT token if available
        if let token = AuthService.shared.authToken {
            request.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")
        }
        
        if let body = body {
            request.httpBody = body
        }
        
        do {
            let (data, response) = try await session.data(for: request)
            
            guard let httpResponse = response as? HTTPURLResponse else {
                throw NetworkError.invalidResponse
            }
            
            guard 200...299 ~= httpResponse.statusCode else {
                throw NetworkError.httpError(httpResponse.statusCode)
            }
            
            let decoder = JSONDecoder()
            decoder.dateDecodingStrategy = .iso8601
            return try decoder.decode(T.self, from: data)
            
        } catch {
            if error is NetworkError {
                throw error
            } else {
                throw NetworkError.requestFailed(error)
            }
        }
    }
    
    // MARK: - User Endpoints
    func getCurrentUser() async throws -> User {
        return try await request(
            endpoint: "/users",
            responseType: [User].self
        ).first ?? User.mockUsers[0] // Fallback to mock data
    }
    
    func getUsers() async throws -> [User] {
        return try await request(
            endpoint: "/users",
            responseType: [User].self
        )
    }
    
    // MARK: - Organization Endpoints
    func getOrganizations() async throws -> [Organization] {
        return try await request(
            endpoint: "/organizations",
            responseType: [Organization].self
        )
    }
    
    func getOrganization(id: UUID) async throws -> Organization {
        return try await request(
            endpoint: "/organizations?id=eq.\(id.uuidString)",
            responseType: [Organization].self
        ).first ?? Organization.mockOrganizations[0]
    }
    
    // MARK: - Project Endpoints
    func getProjects(organizationId: UUID? = nil) async throws -> [Project] {
        var endpoint = "/projects"
        if let orgId = organizationId {
            endpoint += "?organization_id=eq.\(orgId.uuidString)"
        }
        return try await request(
            endpoint: endpoint,
            responseType: [Project].self
        )
    }
    
    func createProject(_ project: Project) async throws -> Project {
        let encoder = JSONEncoder()
        encoder.dateEncodingStrategy = .iso8601
        let data = try encoder.encode(project)
        
        return try await request(
            endpoint: "/projects",
            method: .POST,
            body: data,
            responseType: Project.self
        )
    }
    
    // MARK: - User Organization Endpoints
    func getUserOrganizations(userId: UUID) async throws -> [UserOrganization] {
        return try await request(
            endpoint: "/user_organizations?user_id=eq.\(userId.uuidString)",
            responseType: [UserOrganization].self
        )
    }
}

// MARK: - HTTP Methods
enum HTTPMethod: String {
    case GET = "GET"
    case POST = "POST"
    case PUT = "PUT"
    case DELETE = "DELETE"
}

// MARK: - Network Errors
enum NetworkError: Error, LocalizedError {
    case invalidURL
    case invalidResponse
    case httpError(Int)
    case requestFailed(Error)
    case decodingError(Error)
    
    var errorDescription: String? {
        switch self {
        case .invalidURL:
            return "Invalid URL"
        case .invalidResponse:
            return "Invalid response"
        case .httpError(let code):
            return "HTTP error: \(code)"
        case .requestFailed(let error):
            return "Request failed: \(error.localizedDescription)"
        case .decodingError(let error):
            return "Decoding error: \(error.localizedDescription)"
        }
    }
}
