//
//  Project.swift
//  PQTR
//
//  Created by z on 1/9/2025.
//

import Foundation

struct Project: Codable, Identifiable {
    let id: UUID
    let name: String
    let description: String?
    let organizationId: UUID
    let isActive: Bool
    let createdAt: Date
    let updatedAt: Date
    
    enum CodingKeys: String, CodingKey {
        case id
        case name
        case description
        case organizationId = "organization_id"
        case isActive = "is_active"
        case createdAt = "created_at"
        case updatedAt = "updated_at"
    }
}

// MARK: - Mock Data
extension Project {
    static let mockProjects: [Project] = [
        Project(
            id: UUID(),
            name: "Product Photography",
            description: "Professional product shots for e-commerce",
            organizationId: Organization.mockOrganizations[0].id,
            isActive: true,
            createdAt: Date(),
            updatedAt: Date()
        ),
        Project(
            id: UUID(),
            name: "Portrait Session",
            description: "Corporate headshots and portraits",
            organizationId: Organization.mockOrganizations[1].id,
            isActive: true,
            createdAt: Date(),
            updatedAt: Date()
        ),
        Project(
            id: UUID(),
            name: "Event Coverage",
            description: "Wedding and event photography",
            organizationId: Organization.mockOrganizations[1].id,
            isActive: true,
            createdAt: Date(),
            updatedAt: Date()
        )
    ]
}
