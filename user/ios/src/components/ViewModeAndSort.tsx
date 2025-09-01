// components/ViewModeAndSort.tsx
// View-mode (Grid/List) + Sort By control, designed to match your current card.
// Expo Go friendly: ActionSheetIOS on iOS.

import React, { useMemo } from "react";
import {
  View,
  Text,
  StyleSheet,
  Pressable,
  ActionSheetIOS,
} from "react-native";
import { Ionicons } from "@expo/vector-icons";

export type SortKey = "recent" | "alpha" | "imgs_desc" | "imgs_asc" | "oldest";

export interface ViewModeAndSortProps {
  sort: SortKey;
  onSortChange: (next: SortKey) => void;
  style?: any;
}

const SORT_OPTIONS: { key: SortKey; label: string; icon: keyof typeof Ionicons.glyphMap }[] = [
  { key: "recent",    label: "Recently active",          icon: "time-outline" },
  { key: "alpha",     label: "Alphabetical (A–Z)",       icon: "text-outline" },
  { key: "imgs_desc", label: "Most images",              icon: "images-outline" },
  { key: "imgs_asc",  label: "Fewest images",            icon: "image-outline" },
  { key: "oldest",    label: "Oldest first",             icon: "calendar-outline" },
];

export default function ViewModeAndSort({
  sort,
  onSortChange,
  style,
}: ViewModeAndSortProps) {
  const sortLabel = useMemo(
    () => SORT_OPTIONS.find(o => o.key === sort)?.label ?? "Recently active",
    [sort]
  );

  const openSort = () => {
    ActionSheetIOS.showActionSheetWithOptions(
      {
        title: "Sort projects",
        options: [...SORT_OPTIONS.map(o => o.label), "Cancel"],
        cancelButtonIndex: SORT_OPTIONS.length,
        userInterfaceStyle: "light",
      },
      (idx) => {
        if (idx >= 0 && idx < SORT_OPTIONS.length) {
          onSortChange(SORT_OPTIONS[idx].key);
        }
      }
    );
  };

  return (
    <View style={[styles.container, style]}>
      {/* Sort By */}
      <Pressable style={styles.sortChip} onPress={openSort}>
        <Ionicons name="swap-vertical-outline" size={18} color="#111827" />
        <Text style={styles.sortText} numberOfLines={1}>
          {sortLabel}
        </Text>
        <Ionicons name="chevron-down" size={16} color="#6B7280" />
      </Pressable>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flexDirection: "row",
    alignItems: "center",
  },
  sortChip: {
    flexDirection: "row",
    alignItems: "center",
    gap: 10,
    backgroundColor: "#F1EFEA",
    borderRadius: 10,
    paddingHorizontal: 14,
    paddingVertical: 10,
    borderWidth: StyleSheet.hairlineWidth,
    borderColor: "#ECE7DF",
    minWidth: 140,
  },
  sortText: { 
    flex: 1, 
    color: "#111827", 
    fontWeight: "700",
    fontSize: 14,
  },
});
