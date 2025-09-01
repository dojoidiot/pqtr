# PQTR Automation Settings System

This system provides comprehensive automation settings for photo projects, designed specifically for PQTR's internal workflow and storage infrastructure.

## Features

### 🎨 Processing Automation
- **Default Preset Selection**: Choose a preset to automatically apply to new images
- **Preset Chaining**: Build sequences of presets to apply in order
- **Auto-apply to New**: Automatically process new uploads
- **Retro-apply**: Process existing files in the background

### 📁 File Output Control
- **Export Formats**: JPEG, TIFF, PNG with easy cycling
- **Smart Naming**: Token-based naming rules with live preview
- **Aspect Ratios**: Multiple crop variants for different delivery needs
- **Counter Management**: Customizable file numbering

### 🚀 PQTR Storage Distribution
- **PQTR Archive**: Long-term storage for master files
- **PQTR Client Delivery**: Organized client-ready exports
- **PQTR Newsroom**: Press-ready assets for media
- **PQTR Social Media**: Optimized for social platforms
- **PQTR Print**: High-resolution print-ready files

### 🏷️ Smart Tagging
- **Default Tags**: Pre-configured tags for projects
- **AI Tagging**: Future AI-powered tag suggestions
- **Tag Management**: Easy addition/removal of tags

### 🎯 Quality Control
- **Resolution Checks**: Minimum quality thresholds
- **Exposure Scoring**: Optional quality metrics
- **Auto-flagging**: Automatic low-quality detection

### 💧 Watermarking
- **Flexible Positioning**: 5 position options (TL, TR, BL, BR, Center)
- **Opacity Control**: 0-100% opacity with 5% increments
- **Custom Text**: Branded watermarks for exports

## Usage

### Basic Integration

```tsx
import AutomationSettingsCard from '@/components/AutomationSettingsCard';

// In your project screen
<AutomationSettingsCard 
  projectId="project-123" 
  projectName="Abu Dhabi 2024" 
/>
```

### Store Hook

```tsx
import { useProjectAutomation } from '@/store/projectAutomation';

function MyComponent({ projectId }) {
  const { settings, update, batch } = useProjectAutomation(projectId);
  
  // Update single setting
  update('exportFormat', 'TIFF');
  
  // Batch update multiple settings
  batch({ 
    watermarkEnabled: true, 
    watermarkText: '© PQTR' 
  });
}
```

## File Naming Tokens

The system supports these tokens in naming rules:

- `{project}` - Project name
- `{date}` - Export date (YYYY-MM-DD)
- `{counter}` - Sequential counter (0001, 0002, etc.)
- `{tag}` - Primary tag or "general"
- `{original}` - Original filename

**Example**: `{project}_{date}_{counter}` → `Abu_Dhabi_2024_0001.jpg`

## Distribution Channels

All distribution is handled through PQTR's internal storage infrastructure:

- **Archive**: Master files, high-resolution originals
- **Client Delivery**: Organized exports for client review
- **Newsroom**: Press-ready assets with metadata
- **Social Media**: Optimized for web/social platforms
- **Print**: High-resolution files for print production

## Future Enhancements

- **AI Distribution**: Smart routing based on image content
- **Advanced Scheduling**: Cron-based export schedules
- **Batch Operations**: Bulk preset application
- **Quality Metrics**: Advanced image quality scoring
- **Integration APIs**: Connect with external systems

## Architecture

The system is built with:
- **React Native + Expo**: Cross-platform compatibility
- **TypeScript**: Type-safe development
- **Local State Management**: Simple useState for MVP
- **Modular Components**: Reusable UI components
- **Clean Separation**: Store logic separate from UI

## Customization

### Adding New Distribution Channels

```tsx
// In store/projectAutomation.ts
export type DistChannel = 
  | "PQTR_Archive" 
  | "PQTR_ClientDelivery" 
  | "PQTR_Newsroom" 
  | "PQTR_SocialMedia" 
  | "PQTR_Print"
  | "PQTR_CustomChannel"; // Add your channel
```

### Custom Preset Types

```tsx
// In components/settings/PresetPickerSheet.tsx
const MOCK_PRESETS: Preset[] = [
  { id: "p1", name: "PQTR – Clean Film" },
  { id: "p2", name: "PQTR – Contrast Punch" },
  // Add your custom presets
];
```

## Notes

- This is an MVP implementation with local state
- Replace mock data with real backend integration
- Add persistence layer for settings
- Implement real export pipeline
- Add error handling and validation
- Consider adding undo/redo functionality
