# PQTR iOS App

A professional photography management iOS app built with SwiftUI, designed to work with the PQTR SaaS backend.

## 🏗️ Architecture

The app follows Apple's recommended MVVM (Model-View-ViewModel) architecture with the following structure:

### Models
- **User**: User profile and authentication data
- **Organization**: Multi-tenant organization management
- **Project**: Photography project management
- **UserOrganization**: Role-based access control

### Services
- **NetworkService**: REST API communication with PostgREST backend
- **AuthService**: JWT authentication and secure token storage
- **MockDataService**: Development and testing data

### ViewModels
- **AppViewModel**: Main app state management and data coordination

### Views
- **HomeView**: Dashboard with quick stats and recent activity
- **ProjectsView**: Project listing and management
- **OrganizationsView**: Organization membership and details
- **ProfileView**: User profile and app settings
- **SignInView**: Authentication interface

## 🚀 Features

### Core Functionality
- **Multi-tenant Architecture**: Support for multiple organizations
- **Role-based Access**: Admin, Member, and Viewer roles
- **Project Management**: Create, view, and manage photography projects
- **Secure Authentication**: JWT tokens with Keychain storage
- **Real-time Data**: Pull-to-refresh and automatic updates

### Development Features
- **Mock Data Mode**: Full mock data service for development
- **Network Fallback**: Graceful fallback to mock data when backend unavailable
- **Comprehensive Error Handling**: User-friendly error messages
- **SwiftUI Best Practices**: Modern iOS development patterns

## 🔧 Setup

### Prerequisites
- Xcode 16.0+
- iOS 18.5+
- Swift 5.9+

### Configuration

1. **Backend Connection**: The app is configured to connect to `http://localhost:3000` (PostgREST API)
2. **Mock Data**: By default, the app uses mock data for development. Toggle in Settings to use real backend
3. **Authentication**: Demo credentials are provided in the sign-in screen

### Demo Credentials
- **Email**: `demo@pqtr.ai`
- **Password**: `demo123`

## 📱 App Structure

### Tab Navigation
1. **Home**: Dashboard with statistics and recent activity
2. **Projects**: Project listing, creation, and management
3. **Organizations**: Organization membership and details
4. **Profile**: User settings and app configuration

### Key Screens
- **Sign In**: Authentication with demo credentials
- **Home Dashboard**: Quick stats, recent projects, and organizations
- **Project Detail**: Individual project information and management
- **Organization Detail**: Organization info and associated projects
- **Create Project**: New project creation with organization selection
- **Settings**: App preferences and development options

## 🔐 Security

### Authentication
- JWT token-based authentication
- Secure token storage using iOS Keychain
- Automatic token refresh and validation

### Data Protection
- Row Level Security (RLS) support
- Secure API communication
- Local data encryption

## 🛠️ Development

### Mock Data
The app includes comprehensive mock data for development:
- 2 sample users
- 2 sample organizations
- 3 sample projects
- Role-based user-organization relationships

### Network Layer
- Generic REST API client
- Automatic JSON encoding/decoding
- Comprehensive error handling
- JWT token management

### State Management
- ObservableObject-based state management
- Reactive UI updates
- Centralized data coordination

## 🔄 Backend Integration

### API Endpoints
The app integrates with the PostgREST backend:
- `GET /users` - User management
- `GET /organizations` - Organization listing
- `GET /projects` - Project management
- `POST /projects` - Project creation
- `GET /user_organizations` - Role management

### Data Flow
1. User signs in → JWT token stored in Keychain
2. App loads data from backend (or mock service)
3. Real-time updates via pull-to-refresh
4. CRUD operations with optimistic updates

## 📊 Data Models

### User
```swift
struct User {
    let id: UUID
    let email: String
    let firstName: String?
    let lastName: String?
    let isActive: Bool
    let createdAt: Date
    let updatedAt: Date
}
```

### Organization
```swift
struct Organization {
    let id: UUID
    let name: String
    let slug: String
    let description: String?
    let isActive: Bool
    let createdAt: Date
    let updatedAt: Date
}
```

### Project
```swift
struct Project {
    let id: UUID
    let name: String
    let description: String?
    let organizationId: UUID
    let isActive: Bool
    let createdAt: Date
    let updatedAt: Date
}
```

## 🎨 UI/UX

### Design Principles
- **Apple Human Interface Guidelines**: Native iOS design patterns
- **Accessibility**: VoiceOver and Dynamic Type support
- **Responsive Design**: Adaptive layouts for different screen sizes
- **Consistent Navigation**: Tab-based navigation with clear hierarchy

### Visual Elements
- **SF Symbols**: Consistent iconography
- **Color Scheme**: Blue accent color with semantic colors
- **Typography**: System fonts with proper hierarchy
- **Spacing**: Consistent padding and margins

## 🧪 Testing

### Development Testing
- Mock data service for offline development
- Comprehensive error scenarios
- Network failure handling
- Authentication flow testing

### Production Readiness
- Error handling and user feedback
- Loading states and progress indicators
- Secure data storage
- Performance optimization

## 🚀 Deployment

### Build Configuration
- **Bundle ID**: `co.horyzon.pqtr-ios`
- **Deployment Target**: iOS 18.5+
- **Development Team**: Configured for signing
- **App Category**: Photography

### Production Checklist
- [ ] Backend API endpoints configured
- [ ] JWT authentication working
- [ ] Push notifications (if needed)
- [ ] App Store assets prepared
- [ ] Privacy policy and terms of service

## 📝 Next Steps

### Immediate Development
1. **Image Management**: Photo upload and gallery features
2. **Collaboration**: Team member invitations and permissions
3. **Export Features**: Project export and sharing
4. **Offline Support**: Core data persistence

### Future Enhancements
1. **Push Notifications**: Real-time updates
2. **Advanced Filtering**: Search and filter capabilities
3. **Analytics**: Usage tracking and insights
4. **Integration**: Third-party service connections

## 🔗 Related Documentation

- [SaaS Backend Documentation](../saas/README.md)
- [Project Overview](../../README.md)
- [API Documentation](../saas/DEPLOYMENT.md)

---

Built with ❤️ using SwiftUI and following Apple's best practices for iOS development.
