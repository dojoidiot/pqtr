// components/ProjectListItem.tsx
import React from "react";
import {
  View,
  Text,
  StyleSheet,
  Pressable,
  ViewStyle,
  GestureResponderEvent,
} from "react-native";
import { Ionicons } from "@expo/vector-icons";

type SyncState = "synced" | "uploading" | "queued" | "paused" | "error";

export type ProjectListItemProps = {
  title: string;
  imagesCount: number;
  lastSyncedLabel?: string;   // e.g. "9m ago"
  createdLabel?: string;      // e.g. "Aug 19, 2025"
  syncState: SyncState;
  progress?: number;          // 0..1 (when uploading)
  onPress?: (e: GestureResponderEvent) => void;
  onShare?: () => void;
  onMore?: () => void;
  style?: ViewStyle;
};

/** Compact status chip aligned to the title baseline */
function StatusChip({
  state,
  progress = 0,
}: { state: SyncState; progress?: number }) {
  const pct = Math.max(0, Math.min(1, progress));
  const isSynced = state === "synced" || pct >= 1;

  const label =
    isSynced ? "Synced"
    : state === "uploading" ? `${Math.round(pct * 100)}%`
    : state === "queued" ? "Queued"
    : state === "paused" ? "Paused"
    : "Error";

  const bg =
    isSynced ? "#2D9C5A"
    : state === "error" ? "#DC2626"
    : state === "paused" ? "#9CA3AF"
    : "#1E8E3E";

  return (
    <View style={[styles.chip, { backgroundColor: bg }]}>
      <Text style={styles.chipText}>{label}</Text>
    </View>
  );
}

export default function ProjectListItem({
  title,
  imagesCount,
  lastSyncedLabel,
  createdLabel,
  syncState,
  progress = 0,
  onPress,
  onShare,
  onMore,
  style,
}: ProjectListItemProps) {
  return (
    <Pressable 
      onPress={onPress} 
      style={({ pressed }) => [
        styles.card, 
        style,
        pressed && styles.cardPressed
      ]}
    >
      {/* ROW 1: Title (left) + Utilities (right) */}
      <View style={styles.rowTop}>
        <Text style={styles.title} numberOfLines={1}>{title}</Text>

        <View style={styles.rightCluster}>
          <StatusChip state={syncState} progress={progress} />

          <Pressable hitSlop={8} onPress={onShare} style={styles.iconBtn}>
            <Ionicons name="share-outline" size={18} color="#6B7280" />
          </Pressable>

          <Pressable hitSlop={8} onPress={onMore} style={styles.iconBtn}>
            <Ionicons name="ellipsis-horizontal" size={18} color="#6B7280" />
          </Pressable>
        </View>
      </View>

      {/* ROW 2: Primary metadata */}
      <View style={styles.metaRow}>
        <Text style={styles.metaPri}>{imagesCount.toLocaleString()} images</Text>
        {lastSyncedLabel ? (
          <>
            <Text style={styles.dot}> • </Text>
            <Text style={styles.metaPri}>Last synced {lastSyncedLabel}</Text>
          </>
        ) : null}
      </View>

      {/* ROW 3: Secondary metadata (optional) */}
      {createdLabel ? (
        <Text style={styles.metaSec}>Created {createdLabel}</Text>
      ) : null}
    </Pressable>
  );
}

const R = 16;

const styles = StyleSheet.create({
  card: {
    backgroundColor: "#fff",
    borderRadius: R,
    borderWidth: StyleSheet.hairlineWidth,
    borderColor: "#E9E5DE",
    paddingHorizontal: 16,
    paddingVertical: 12,
    // Top alignment (we manage our own vertical rhythm)
    alignItems: "stretch",
    shadowColor: "#000",
    shadowOpacity: 0.05,
    shadowRadius: 6,
    shadowOffset: { width: 0, height: 2 },
  },
  cardPressed: {
    opacity: 0.8,
    transform: [{ scale: 0.98 }],
    backgroundColor: "#F8F7F4",
  },

  /** Row 1 */
  rowTop: {
    flexDirection: "row",
    alignItems: "center", // aligns chip to title baseline area
  },
  title: {
    flexShrink: 1,
    fontSize: 18,
    fontWeight: "800",
    color: "#111827",
    paddingRight: 12,
  },
  rightCluster: {
    marginLeft: "auto",
    flexDirection: "row",
    alignItems: "center",
    gap: 8,
  },

  /** Row 2 */
  metaRow: {
    flexDirection: "row",
    alignItems: "center",
    marginTop: 6,
  },
  metaPri: { color: "#4B5563", fontSize: 13, fontWeight: "600" },
  dot: { color: "#9CA3AF", fontSize: 13, paddingHorizontal: 4 },

  /** Row 3 */
  metaSec: { color: "#9CA3AF", fontSize: 12, marginTop: 2 },

  /** Utilities */
  chip: {
    paddingHorizontal: 12,
    paddingVertical: 6,
    borderRadius: 999,
  },
  chipText: { color: "#fff", fontWeight: "800", fontSize: 12 },
  iconBtn: {
    width: 32, height: 32, borderRadius: 8,
    alignItems: "center", justifyContent: "center",
    backgroundColor: "#F3F2EE",
  },
});
