//
//  Organization.swift
//  PQTR
//
//  Created by z on 1/9/2025.
//

import Foundation

struct Organization: Codable, Identifiable, Hashable {
    let id: UUID
    let name: String
    let slug: String
    let description: String?
    let isActive: Bool
    let createdAt: Date
    let updatedAt: Date
    
    enum CodingKeys: String, CodingKey {
        case id
        case name
        case slug
        case description
        case isActive = "is_active"
        case createdAt = "created_at"
        case updatedAt = "updated_at"
    }
}

// MARK: - Mock Data
extension Organization {
    static let mockOrganizations: [Organization] = [
        Organization(
            id: UUID(),
            name: "Acme Corporation",
            slug: "acme-corp",
            description: "Leading technology company",
            isActive: true,
            createdAt: Date(),
            updatedAt: Date()
        ),
        Organization(
            id: UUID(),
            name: "Creative Studio",
            slug: "creative-studio",
            description: "Design and photography studio",
            isActive: true,
            createdAt: Date(),
            updatedAt: Date()
        )
    ]
}
