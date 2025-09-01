# PhotoCard & ProjectMasonrySection Components

This document describes the new PhotoCard and ProjectMasonrySection components that provide a clean, minimal-by-default photo grid experience with two distinct modes.

## Components Overview

### PhotoCard
A minimal-by-default photo card component that supports two interaction modes:
- **Browse Mode**: First tap shows title overlay, second tap opens details
- **PresetSelect Mode**: Tap toggles selection with large checkmark

### ProjectMasonrySection
A masonry grid component that renders PhotoCards in a responsive grid layout with support for both modes.

## Features

#### PhotoCard Features
- **Minimal Design**: Clean, uncluttered appearance by default
- **Status Indicator**: Small colored dot showing sync/publish status
- **Smart Interactions**: Different behavior based on mode
- **Auto-hide Overlay**: Title overlay automatically disappears after 1.8 seconds in browse mode
- **Selection Support**: Large checkmark and selection ring in presetSelect mode

#### ProjectMasonrySection Features
- **Responsive Grid**: Automatically adjusts to screen width
- **Masonry Layout**: Photos maintain aspect ratio while filling available space
- **Mode Switching**: Seamlessly switch between browse and presetSelect modes
- **Selection Management**: Built-in selection state with callback support
- **Performance Optimized**: Uses FlatList for efficient rendering

## Usage

### Basic PhotoCard Usage

```tsx
import PhotoCard from './components/PhotoCard';

<PhotoCard
  id="photo-1"
  uri="https://example.com/photo.jpg"
  width={1200}
  height={800}
  title="Beautiful Landscape"
  status="synced"
  mode="browse"
  onOpenDetail={(id) => console.log('Opening:', id)}
/>
```

### Basic ProjectMasonrySection Usage

```tsx
import ProjectMasonrySection from './components/ProjectMasonrySection';

<ProjectMasonrySection
  mode="browse"
  data={photos}
  columns={2}
  onSelectionChange={(ids) => console.log('Selected:', ids)}
  onOpenDetail={(id) => console.log('Opening:', id)}
/>
```

## Modes

### Browse Mode (`mode="browse"`)
- **First Tap**: Shows title overlay with "Tap again for details" hint
- **Second Tap**: Opens photo details (calls `onOpenDetail`)
- **Auto-hide**: Overlay disappears after 1.8 seconds
- **Visual**: Only shows status dot, no selection UI

### PresetSelect Mode (`mode="presetSelect"`)
- **Tap**: Toggles photo selection
- **Visual**: Shows large checkmark when selected, selection ring around card
- **Selection**: Calls `onSelectionChange` with array of selected IDs
- **No Overlay**: Title overlay is disabled in this mode

## Props

### PhotoCard Props

```tsx
type PhotoCardProps = {
  id: string;                    // Unique identifier
  uri: string;                   // Image URI
  width: number;                 // Original image width
  height: number;                // Original image height
  title: string;                 // Photo title
  status?: Status;               // Sync/publish status
  mode: Mode;                    // "browse" | "presetSelect"
  selected?: boolean;            // Selection state (presetSelect mode)
  onToggleSelect?: (id: string) => void;  // Selection callback
  onOpenDetail?: (id: string) => void;    // Detail open callback
};
```

### ProjectMasonrySection Props

```tsx
type Props = {
  data: Photo[];                 // Array of photo objects
  mode: Mode;                    // "browse" | "presetSelect"
  columns?: 2 | 3;              // Grid columns (default: 2)
  onSelectionChange?: (ids: string[]) => void;  // Selection callback
  onOpenDetail?: (id: string) => void;          // Detail open callback
};
```

## Status Types

```tsx
type Status = 
  | "synced"      // Green dot - photo is synced
  | "syncing"     // Yellow dot - photo is currently syncing
  | "needsSync"   // Red dot - photo needs to be synced
  | "error"       // Red dot - sync error occurred
  | "published"   // Green dot - photo is published
```

## Integration with ProjectScreen

The ProjectScreen has been updated to support both modes:

```tsx
// Mode state
const [mode, setMode] = useState<Mode>("browse");
const [selectedIds, setSelectedIds] = useState<string[]>([]);

// Mode toggle buttons
<Pressable onPress={() => setMode("browse")}>
  <Text>Browse</Text>
</Pressable>
<Pressable onPress={() => setMode("presetSelect")}>
  <Text>Preset</Text>
</Pressable>

// Masonry section
<ProjectMasonrySection
  mode={mode}
  data={transformedPhotos}
  onSelectionChange={setSelectedIds}
  onOpenDetail={(id) => navigation.navigate("ImageDetail", { id })}
/>
```

## Demo Component

A demo component (`PhotoCardDemo.tsx`) is included to showcase both modes:

- Switch between browse and presetSelect modes
- See selection state in real-time
- Test tap interactions
- View sample photos with different statuses

## Styling

The components use a consistent design system:
- **Colors**: Matches the existing PQTR theme
- **Border Radius**: 14px for cards
- **Spacing**: 16px padding, 12px gutters
- **Typography**: Consistent with app font weights and sizes

## Performance Notes

- Uses `useCallback` for event handlers to prevent unnecessary re-renders
- `useMemo` for expensive calculations (grid layout, photo transformations)
- FlatList for efficient scrolling and rendering
- Minimal re-renders when switching modes

## Migration from Old Components

The new components replace the previous complex masonry system:
- **Removed**: Complex badge stacks, metadata peek modals
- **Simplified**: Photo data structure (removed unused fields)
- **Streamlined**: Single interaction pattern per mode
- **Consistent**: Unified design language across modes

## Future Enhancements

Potential improvements for future versions:
- **Custom Overlays**: Allow custom content in title overlay
- **Animation**: Smooth transitions between modes
- **Accessibility**: Screen reader support and focus management
- **Theming**: Customizable color schemes and styles
