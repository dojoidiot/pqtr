// components/PhotoCard.tsx
// Minimal-by-default card. First tap = show title overlay. Second tap = open details.
// In presetSelect mode, tapping toggles selection (large check). No mini buttons on the card.

import React, { useState, useCallback, useEffect } from "react";
import { View, Text, Image, Pressable, StyleSheet } from "react-native";

type Status = "synced" | "syncing" | "needsSync" | "error" | "published";
type Mode = "browse" | "presetSelect";

export type PhotoCardProps = {
  id: string;
  uri: string;
  width: number;
  height: number;
  title: string;
  status?: Status;
  mode: Mode;
  selected?: boolean;
  onToggleSelect?: (id: string) => void;
  onOpenDetail?: (id: string) => void;
};

export default function PhotoCard({
  id,
  uri,
  width,
  height,
  title,
  status,
  mode,
  selected = false,
  onToggleSelect,
  onOpenDetail,
}: PhotoCardProps) {
  const aspect = Math.max(0.1, width / Math.max(1, height));
  const cardW = undefined; // parent sets width
  const cardH = undefined; // parent sets height

  const [overlayVisible, setOverlayVisible] = useState(false);

  // Auto-hide overlay after a short delay (optional)
  useEffect(() => {
    if (overlayVisible && mode === "browse") {
      const t = setTimeout(() => setOverlayVisible(false), 1800);
      return () => clearTimeout(t);
    }
  }, [overlayVisible, mode]);

  const statusColor =
    status === "synced" || status === "published" ? "#2D9C5A"
      : status === "syncing" ? "#F59E0B"
      : status === "error" ? "#DC2626"
      : "#A8A29E";

  const handlePress = useCallback(() => {
    if (mode === "presetSelect") {
      onToggleSelect?.(id);
      return;
    }
    // browse mode
    if (!overlayVisible) setOverlayVisible(true);
    else onOpenDetail?.(id);
  }, [mode, overlayVisible, id, onToggleSelect, onOpenDetail]);

  return (
    <Pressable onPress={handlePress} style={[styles.card]}>
      <Image source={{ uri }} style={styles.image} resizeMode="cover" />

      {/* tiny status dot only (default) */}
      <View style={styles.topLeft}>
        <View style={[styles.dot, { backgroundColor: statusColor }]} />
      </View>

      {/* selection check (only in presetSelect) */}
      {mode === "presetSelect" && (
        <View style={styles.topRight}>
          <View style={[styles.check, selected && styles.checkOn]}>
            {selected && <Text style={styles.checkTick}>✓</Text>}
          </View>
        </View>
      )}

      {/* title overlay (browse only, on first tap) */}
      {mode === "browse" && overlayVisible && (
        <>
          <View style={styles.scrim} />
          <View style={styles.titleWrap}>
            <Text numberOfLines={1} style={styles.titleText}>{title}</Text>
            <Text style={styles.subtleText}>Tap again for details</Text>
          </View>
        </>
      )}

      {/* selection ring */}
      {mode === "presetSelect" && selected && <View style={styles.selRing} />}
    </Pressable>
  );
}

const R = 14;
const styles = StyleSheet.create({
  card: {
    borderRadius: R,
    overflow: "hidden",
    backgroundColor: "#E7E1D7",
    width: "100%",
    height: "100%",
  },
  image: {
    width: "100%",
    height: "100%",
  },
  topLeft: { position: "absolute", top: 8, left: 8 },
  dot: { width: 10, height: 10, borderRadius: 999, borderWidth: 1, borderColor: "rgba(255,255,255,0.8)" },

  topRight: { position: "absolute", top: 8, right: 8 },
  check: {
    width: 26, height: 26, borderRadius: 999, borderWidth: 2,
    borderColor: "rgba(255,255,255,0.85)", backgroundColor: "rgba(0,0,0,0.25)",
    alignItems: "center", justifyContent: "center",
  },
  checkOn: { backgroundColor: "#2D9C5A", borderColor: "#2D9C5A" },
  checkTick: { color: "#fff", fontSize: 16, fontWeight: "800" },

  scrim: { ...StyleSheet.absoluteFillObject, backgroundColor: "rgba(0,0,0,0.35)" },
  titleWrap: {
    position: "absolute", left: 10, right: 10, bottom: 10,
    alignItems: "flex-start",
  },
  titleText: { color: "#fff", fontWeight: "800", fontSize: 15, textShadowColor: "rgba(0,0,0,0.3)", textShadowRadius: 6 },
  subtleText: { color: "rgba(255,255,255,0.9)", fontSize: 11, marginTop: 2 },

  selRing: { ...StyleSheet.absoluteFillObject, borderWidth: 2, borderColor: "#2D9C5A", borderRadius: R },
});
