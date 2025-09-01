import React from "react";
import { View, FlatList, StyleSheet, Dimensions, Text } from "react-native";
import { ImageItem } from "../../types/images";
import ImageCard from "./ImageCard";
import { space, colors, text as T } from "../../theme/tokens";

export default function GalleryGrid({
  items, selectedIds, selecting, onToggleSelect, onSelectFirst, onOpenDetail,
  targetCardWidth,                                // NEW
}: {
  items: ImageItem[];
  selectedIds: string[];
  selecting: boolean;                 // NEW
  onToggleSelect: (id: string) => void;
  onSelectFirst: (id: string) => void; // NEW (from long-press)
  onOpenDetail: (id: string) => void;
  targetCardWidth: number;                        // NEW
}) {
  const { width } = Dimensions.get("window");
  const pad = space.lg;
  const gap = space.md;

  // compute the number of columns the screen can fit for the chosen width
  // ensure at least 1 column
  const available = width - pad * 2;
  const cols = Math.max(1, Math.floor((available + gap) / (targetCardWidth + gap)));

  // recompute the actual card width so columns perfectly fit the row
  const cardW = Math.floor((available - gap * (cols - 1)) / cols);

  if (items.length === 0) {
    return (
      <View style={styles.emptyContainer}>
        <Text style={styles.emptyText}>No photos found in this project</Text>
        <Text style={styles.uploadText}>Tap the + button to add photos</Text>
      </View>
    );
  }

  return (
    <FlatList
      data={items}
      key={`cols-${cols}`} // force a new layout when columns change
      keyExtractor={(it) => it.id}
      numColumns={cols}
      columnWrapperStyle={cols > 1 ? { gap } : undefined}
      contentContainerStyle={{ paddingHorizontal: pad, paddingTop: space.md, paddingBottom: 140, gap: space.lg }}
      renderItem={({ item }) => (
        <ImageCard
          item={item}
          width={cardW}
          selected={selectedIds.includes(item.id)}
          selecting={selecting}                  // NEW
          onTap={() => {
            // If NOT in selection mode → open detail.
            // If in selection mode → toggle selection.
            selecting ? onToggleSelect(item.id) : onOpenDetail(item.id);
          }}
          onLongPress={() => onSelectFirst(item.id)} // long-press enters select & selects
        />
      )}
    />
  );
}

const styles = StyleSheet.create({
  emptyContainer: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    backgroundColor: colors.bg,
    paddingHorizontal: space.xl,
    paddingVertical: space.xxl,
  },
  emptyText: {
    fontSize: 16,
    color: colors.subtext,
    textAlign: 'center',
    marginBottom: 8,
  },
  uploadText: {
    fontSize: 14,
    color: colors.mute,
    textAlign: 'center',
  },
});
