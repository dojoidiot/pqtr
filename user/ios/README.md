# PQTR - React Native App

A modern React Native application built with Expo and TypeScript, optimized for iOS MVP development.

## 🚀 Features

- **TypeScript**: Full type safety and better developer experience
- **Expo**: Cross-platform development with easy deployment
- **iOS HIG Compliance**: Follows Apple's Human Interface Guidelines
- **Styled Components**: Modern CSS-in-JS styling approach
- **Navigation**: React Navigation with stack and tab navigation
- **Haptic Feedback**: iOS-like interaction patterns
- **Responsive Design**: Optimized for various iOS device sizes

## 📁 Project Structure

```
/src
  ├── /screens         # All app views (Home, Project, Image, etc.)
  ├── /components      # Reusable UI components
  ├── /navigation      # Navigation logic and stacks
  ├── /assets          # Images, icons, fonts
  ├── /constants       # Colors, spacing, typography
  ├── /types           # Shared interfaces and types
  ├── /hooks           # Custom logic
  ├── /utils           # Helper functions
  └── /mock            # Temporary test data
```

## 🎨 Design System

### Colors
- **Background**: `#F6EFE8` (Warm beige)
- **Accent**: `#1E1E1E` (Dark gray)
- **Green**: `#29694C` (Primary accent)
- **Gray**: `#B3B3B3` (Secondary text)
- **White**: `#FFFFFF` (Cards and buttons)

### Typography
- **Titles**: 20pt bold (SF Pro Text)
- **Body**: 14pt medium
- **Caption**: 12pt regular

### Spacing
- **Small**: 8pt
- **Medium**: 16pt
- **Large**: 24pt

## 🛠️ Installation

1. **Clone the repository**
   ```bash
   git clone <repository-url>
   cd PQTR
   ```

2. **Install dependencies**
   ```bash
   npm install
   ```

3. **Start the development server**
   ```bash
   npm start
   ```

4. **Run on iOS**
   ```bash
   npm run ios
   ```

## 📱 Current Screens

### HomeScreen
- PQTR branding header
- Storage usage indicator with progress bar
- Create project button (+)
- Project list with image thumbnails
- Profile tab at bottom

## 🔧 Dependencies

### Core Navigation
- `@react-navigation/native`
- `@react-navigation/native-stack`
- `@react-navigation/bottom-tabs`

### Expo Modules
- `react-native-screens`
- `react-native-safe-area-context`
- `react-native-gesture-handler`
- `react-native-reanimated`
- `react-native-svg`

### Styling & UI
- `styled-components`
- `@expo/vector-icons`
- `react-native-haptic-feedback`

## 🚧 Development Status

- ✅ Project structure setup
- ✅ Theme and constants
- ✅ TypeScript types
- ✅ HomeScreen component
- ✅ ProjectCard component
- ✅ Mock data
- ✅ Basic navigation
- 🔄 Navigation stack (in progress)
- 🔄 Additional screens (planned)
- 🔄 Image handling (planned)
- 🔄 Project management (planned)

## 📋 Next Steps

1. **Complete Navigation Stack**
   - ProjectScreen
   - ImageScreen
   - ImageDetailsScreen

2. **Add Bottom Tab Navigation**
   - Home tab (current)
   - Profile tab

3. **Implement Core Features**
   - Project creation
   - Image upload
   - Storage management

4. **iOS Optimization**
   - Gesture handling
   - Modal transitions
   - Haptic feedback patterns

## 🎯 iOS MVP Focus

This app is specifically designed for iOS MVP development with:
- Native iOS design patterns
- SF Pro Text typography
- iOS spacing guidelines (8pt grid)
- Haptic feedback integration
- Safe area handling
- iOS navigation patterns

## 📄 License

This project is proprietary and confidential. 