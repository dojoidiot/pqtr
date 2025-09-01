// File: components/ProjectMasonrySection.tsx
// Purpose: Replace the confusing full-width image area under Filters with a premium masonry grid.
// Dependencies: React Native + Expo Go (no native deps). Icons via @expo/vector-icons.
// Usage: Render <ProjectMasonrySection mode="browse" data={PHOTOS}/> inside your Project screen below Filters.
// Modes: "browse" | "presetSelect" (browse = tap to show title, presetSelect = tap to toggle selection).

import React, { useMemo, useState, useCallback } from "react";
import { View, FlatList, StyleSheet, Dimensions } from "react-native";
import PhotoCard from "./PhotoCard";

type Mode = "browse" | "presetSelect";

type Photo = {
  id: string;
  uri: string;
  width: number;
  height: number;
  title: string;
  status?: "synced" | "syncing" | "needsSync" | "error" | "published";
};

export default function ProjectMasonrySection({
  data,
  mode,
  columns = 2,
  onSelectionChange,
  onOpenDetail,
}: {
  data: Photo[];
  mode: Mode;
  columns?: 2 | 3;
  onSelectionChange?: (ids: string[]) => void;
  onOpenDetail?: (id: string) => void;
}) {
  const [selected, setSelected] = useState<Set<string>>(new Set());

  const toggle = useCallback((id: string) => {
    setSelected(prev => {
      const n = new Set(prev);
      n.has(id) ? n.delete(id) : n.add(id);
      onSelectionChange?.(Array.from(n));
      return n;
    });
  }, [onSelectionChange]);

  // Calculate proper masonry layout with adequate spacing
  const { columnData, columnWidth } = useMemo(() => {
    const screenWidth = Dimensions.get("window").width;
    const horizontalPadding = 16;
    const gutter = 12;
    const availableWidth = screenWidth - (horizontalPadding * 2) - (gutter * (columns - 1));
    const columnWidth = availableWidth / columns;



    // Create columns array
    const columnArray: { items: (Photo & { calculatedHeight: number })[]; totalHeight: number }[] = 
      Array.from({ length: columns }, () => ({ items: [], totalHeight: 0 }));

    // Distribute items across columns based on height
    data.forEach((photo, index) => {
      // Calculate aspect ratio and height for this column width
      const aspectRatio = photo.width / photo.height;
      const calculatedHeight = columnWidth / aspectRatio;
      

      
      // Find the shortest column to maintain balance
      let shortestColumnIndex = 0;
      let shortestHeight = columnArray[0].totalHeight;
      
      for (let i = 1; i < columnArray.length; i++) {
        if (columnArray[i].totalHeight < shortestHeight) {
          shortestHeight = columnArray[i].totalHeight;
          shortestColumnIndex = i;
        }
      }

      // Add item to shortest column
      columnArray[shortestColumnIndex].items.push({
        ...photo,
        calculatedHeight
      });
      columnArray[shortestColumnIndex].totalHeight += calculatedHeight + gutter;
      

    });



    return { columnData: columnArray, columnWidth };
  }, [data, columns]);

  return (
    <View style={styles.container}>
      <FlatList
        data={columnData}
        keyExtractor={(_, index) => `column-${index}`}
        horizontal
        scrollEnabled={false}
        showsHorizontalScrollIndicator={false}
        contentContainerStyle={styles.masonryContainer}
        renderItem={({ item: column, index }) => (
          <View style={[
            styles.column, 
            { 
              width: columnWidth,
              marginRight: index === columnData.length - 1 ? 0 : 12
            }
          ]}>
            {column.items.map((photo, photoIndex) => (
              <View 
                key={photo.id} 
                style={[
                  styles.photoWrapper,
                  { 
                    height: photo.calculatedHeight,
                    marginBottom: photoIndex === column.items.length - 1 ? 0 : 12
                  }
                ]}
              >
                <PhotoCard
                  id={photo.id}
                  uri={photo.uri}
                  width={photo.width}
                  height={photo.height}
                  title={photo.title}
                  status={photo.status}
                  mode={mode}
                  selected={selected.has(photo.id)}
                  onToggleSelect={toggle}
                  onOpenDetail={onOpenDetail}
                />
              </View>
            ))}
          </View>
        )}
      />
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
  },
  masonryContainer: {
    paddingHorizontal: 16,
    paddingBottom: 24,
  },
  column: {
    flex: 1,
  },
  photoWrapper: {
    width: '100%',
    borderRadius: 14,
    overflow: 'hidden',
  },
});
