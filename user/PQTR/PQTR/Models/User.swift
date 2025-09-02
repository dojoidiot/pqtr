//
//  User.swift
//  PQTR
//
//  Created by z on 1/9/2025.
//

import Foundation

struct User: Codable, Identifiable {
    let id: UUID
    let email: String
    let firstName: String?
    let lastName: String?
    let isActive: Bool
    let createdAt: Date
    let updatedAt: Date
    
    enum CodingKeys: String, CodingKey {
        case id
        case email
        case firstName = "first_name"
        case lastName = "last_name"
        case isActive = "is_active"
        case createdAt = "created_at"
        case updatedAt = "updated_at"
    }
    
    var fullName: String {
        let first = firstName ?? ""
        let last = lastName ?? ""
        return "\(first) \(last)".trimmingCharacters(in: .whitespaces)
    }
}

// MARK: - Mock Data
extension User {
    static let mockUsers: [User] = [
        User(
            id: UUID(),
            email: "john.doe@example.com",
            firstName: "John",
            lastName: "Doe",
            isActive: true,
            createdAt: Date(),
            updatedAt: Date()
        ),
        User(
            id: UUID(),
            email: "jane.smith@example.com",
            firstName: "Jane",
            lastName: "Smith",
            isActive: true,
            createdAt: Date(),
            updatedAt: Date()
        )
    ]
}
