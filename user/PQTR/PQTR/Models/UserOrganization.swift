//
//  UserOrganization.swift
//  PQTR
//
//  Created by z on 1/9/2025.
//

import Foundation

struct UserOrganization: Codable, Identifiable {
    let id: UUID
    let userId: UUID
    let organizationId: UUID
    let role: Role
    let createdAt: Date
    
    enum CodingKeys: String, CodingKey {
        case id
        case userId = "user_id"
        case organizationId = "organization_id"
        case role
        case createdAt = "created_at"
    }
}

enum Role: String, Codable, CaseIterable {
    case admin = "admin"
    case member = "member"
    case viewer = "viewer"
    
    var displayName: String {
        switch self {
        case .admin: return "Admin"
        case .member: return "Member"
        case .viewer: return "Viewer"
        }
    }
}

// MARK: - Mock Data
extension UserOrganization {
    static let mockUserOrganizations: [UserOrganization] = [
        UserOrganization(
            id: UUID(),
            userId: User.mockUsers[0].id,
            organizationId: Organization.mockOrganizations[0].id,
            role: .admin,
            createdAt: Date()
        ),
        UserOrganization(
            id: UUID(),
            userId: User.mockUsers[0].id,
            organizationId: Organization.mockOrganizations[1].id,
            role: .member,
            createdAt: Date()
        ),
        UserOrganization(
            id: UUID(),
            userId: User.mockUsers[1].id,
            organizationId: Organization.mockOrganizations[1].id,
            role: .admin,
            createdAt: Date()
        )
    ]
}
