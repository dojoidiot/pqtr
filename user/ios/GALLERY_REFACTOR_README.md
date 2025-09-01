# Gallery Refactor - Project Page Architecture

## Overview
The Project page has been completely refactored to provide a clean, modern, and intuitive flow for browsing, selecting, applying presets, and sharing images.

## New Architecture

### Core Components
- **ProjectScreen**: Main screen orchestrating all gallery functionality
- **GalleryFilters**: Segmented filters and search functionality
- **GalleryGrid**: Responsive image grid with selection support
- **ImageCard**: Individual image display with status indicators
- **StickyActionBar**: Context-aware action bar for selected/unselected states

### Sheets & Modals
- **PresetPickerSheet**: Modal for selecting and applying presets
- **ShareSheet**: Modal for sharing options
- **MoreMenu**: Context menu with additional options

### State Management
- **selectionStore**: Zustand store for managing image selection state
- **useSelection**: Hook for accessing selection state and actions

### Design System
- **theme/tokens.ts**: Centralized design tokens (colors, spacing, typography, shadows)
- **types/images.ts**: TypeScript interfaces for image data

## Key Features

### 1. Segmented Filters
- All, Favorites, Processed, Unprocessed
- Clean toggle interface with active states

### 2. Search Functionality
- Real-time search across filenames and tags
- Integrated with filter system

### 3. Context-Aware Selection
- Tap to select/deselect images
- Long press to enter selection mode
- Visual feedback with checkmarks and overlays

### 4. Sticky Action Bar
- **No Selection**: Preset, Share, More options
- **With Selection**: Selection count, Apply, Share, Clear

### 5. More Menu Integration
- Automation Settings moved to More menu
- Select All/Deselect All options
- View and sort controls

## Component Structure

```
src/
├── components/
│   ├── gallery/
│   │   ├── GalleryFilters.tsx      # Filter tabs + search
│   │   ├── GalleryGrid.tsx         # Image grid container
│   │   ├── ImageCard.tsx           # Individual image
│   │   ├── StickyActionBar.tsx     # Context actions
│   │   └── index.ts                # Clean exports
│   ├── sheets/
│   │   ├── PresetPickerSheet.tsx   # Preset selection
│   │   ├── ShareSheet.tsx          # Sharing options
│   │   └── index.ts                # Clean exports
│   └── menus/
│       ├── MoreMenu.tsx            # Context menu
│       └── index.ts                # Clean exports
├── store/
│   └── selectionStore.ts           # Selection state management
├── theme/
│   └── tokens.ts                   # Design system
├── types/
│   └── images.ts                   # TypeScript interfaces
└── utils/
    └── toast.ts                    # Cross-platform notifications
```

## Usage

### Basic Implementation
```tsx
import { useSelection } from '../store/selectionStore';
import { GalleryFilters, GalleryGrid, StickyActionBar } from '../components/gallery';

function MyGallery() {
  const { selectedIds, toggle, clear } = useSelection();
  
  return (
    <View>
      <GalleryFilters
        filter={filter}
        onChangeFilter={setFilter}
        query={searchQuery}
        onChangeQuery={setSearchQuery}
        onUpload={handleUpload}
      />
      
      <GalleryGrid
        items={images}
        selectedIds={selectedIds}
        onToggleSelect={toggle}
        onOpenDetail={handleOpenDetail}
        onEnterSelectMode={handleEnterSelectMode}
      />
      
      <StickyActionBar
        selectedCount={selectedIds.length}
        onPreset={handlePreset}
        onShare={handleShare}
        onClear={clear}
        onMore={handleMore}
      />
    </View>
  );
}
```

## Design Principles

### Accessibility
- Large hit targets (≥44pt)
- Proper accessibility roles and labels
- Screen reader support

### Performance
- Minimal animations (slide/scale/fade)
- Efficient re-renders with useCallback/useMemo
- Optimized image loading

### UX Patterns
- iOS-style spacing and radii
- Subtle shadows and depth
- Haptic feedback integration
- Toast notifications for user feedback

## Migration Notes

### Removed Features
- Old "Browse/Preset" toggle system
- Inline automation settings button
- Complex nested FlatList structure

### New Features
- Segmented filter system
- Context-aware action bar
- Integrated search and filtering
- Modern sheet-based modals

## Dependencies
- **zustand**: State management
- **@expo/vector-icons**: Icon system
- **expo-haptics**: Haptic feedback
- **react-navigation**: Navigation system

## Future Enhancements
- Grid/List view toggle
- Advanced sorting options
- Batch operations
- Drag and drop reordering
- Image editing integration
