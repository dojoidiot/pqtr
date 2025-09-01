// File: components/ProjectCard.tsx
// A sleek, modern project card with glass chips, subtle scrims, and optional progress.
// Props cover all needed states without visual clutter.

import React from "react";
import {
  View,
  Text,
  Image,
  StyleSheet,
  Pressable,
  ViewStyle,
} from "react-native";
import { BlurView } from "expo-blur";
import { Ionicons } from "@expo/vector-icons";

type SyncState = "synced" | "uploading" | "queued" | "paused" | "error";

export type ProjectCardProps = {
  coverUri: string;
  title: string;
  imagesCount: number;
  lastUpdated?: string; // "Aug 19, 2025"
  syncState: SyncState;
  progress?: number; // 0..1
  onPress?: () => void;
  onMenuPress?: () => void;
  style?: ViewStyle;
};

export default function ProjectCard({
  coverUri,
  title,
  imagesCount,
  lastUpdated,
  syncState,
  progress = 0,
  onPress,
  onMenuPress,
  style,
}: ProjectCardProps) {
  const isSynced = syncState === "synced" || progress >= 1;

  return (
    <Pressable 
      onPress={onPress} 
      style={({ pressed }) => [
        styles.card, 
        style,
        pressed && styles.cardPressed
      ]}
    >
      <Image source={{ uri: coverUri }} style={styles.image} resizeMode="cover" />

      {/* Top-left status chip (glass) */}
      <BlurView intensity={28} tint="dark" style={styles.statusChip}>
        <View
          style={[
            styles.dot,
            { backgroundColor:
              isSynced ? "#2D9C5A" :
              syncState === "error" ? "#DC2626" :
              syncState === "paused" ? "#9CA3AF" :
              "#1E8E3E"
            },
          ]}
        />
        <Text style={styles.statusText}>
          {isSynced ? "Synced" :
           syncState === "uploading" ? `${Math.round(progress * 100)}%` :
           syncState.charAt(0).toUpperCase() + syncState.slice(1)}
        </Text>
        {/* Progress bar within status chip for uploading/syncing */}
        {!isSynced && (syncState === "uploading" || syncState === "queued") && (
          <View style={styles.inlineProgressTrack}>
            <View style={[styles.inlineProgressFill, { width: `${Math.round(progress * 100)}%` }]} />
          </View>
        )}
      </BlurView>

      {/* Title + meta */}
      <View style={styles.metaBlock}>
        <Text style={styles.title} numberOfLines={1}>{title}</Text>
        <Text style={styles.meta} numberOfLines={1}>
          {imagesCount.toLocaleString()} images
          {lastUpdated ? `  •  ${lastUpdated}` : ""}
        </Text>
      </View>

      {/* Right menu button */}
      <Pressable onPress={onMenuPress} hitSlop={8}>
        <View style={styles.menuWrap}>
          <Ionicons name="ellipsis-horizontal" size={20} color="#fff" />
          <Text style={styles.debugText}>•••</Text>
        </View>
      </Pressable>
      
      {/* Debug button - temporary */}
      <View style={styles.debugButton}>
        <Text style={styles.debugButtonText}>MENU</Text>
      </View>
    </Pressable>
  );
}

const R = 18;

const styles = StyleSheet.create({
  card: {
    height: 168,
    width: '100%',
    borderRadius: R,
    overflow: "hidden",
    backgroundColor: "#EDE8E1",
  },
  cardPressed: {
    opacity: 0.8,
    transform: [{ scale: 0.98 }],
  },
  image: {
    ...StyleSheet.absoluteFillObject as any,
    width: "100%",
    height: "100%",
  },
  statusChip: {
    position: "absolute", top: 10, left: 10,
    paddingHorizontal: 10, paddingVertical: 8, borderRadius: 999,
    flexDirection: "row", alignItems: "center", gap: 8,
    borderWidth: StyleSheet.hairlineWidth, borderColor: "rgba(255,255,255,0.15)",
    minHeight: 32, // Ensure enough height for progress bar
  },
  dot: { width: 8, height: 8, borderRadius: 99 },
  statusText: { color: "#fff", fontWeight: "700", fontSize: 12, letterSpacing: 0.2 },

  metaBlock: {
    position: "absolute", left: 14, right: 60, bottom: 26,
  },
  title: {
    color: "#fff", fontSize: 22, fontWeight: "800",
    textShadowColor: "rgba(0,0,0,0.6)", textShadowOffset: { width: 0, height: 1 }, textShadowRadius: 4,
  },
  meta: {
    marginTop: 4, color: "rgba(255,255,255,0.95)", fontSize: 13, fontWeight: "600",
    textShadowColor: "rgba(0,0,0,0.5)", textShadowOffset: { width: 0, height: 1 }, textShadowRadius: 2,
  },

  menuWrap: {
    position: "absolute", right: 12, bottom: 12,
    width: 36, height: 36, borderRadius: 999, alignItems: "center", justifyContent: "center",
    backgroundColor: "red", // Temporary bright red for debugging
    borderWidth: 2, borderColor: "#fff",
    shadowColor: "#000", shadowOffset: { width: 0, height: 2 }, shadowOpacity: 0.8, shadowRadius: 4,
    elevation: 10,
    zIndex: 1000, // Ensure it's on top
  },
  inlineProgressTrack: {
    position: "absolute", bottom: 4, left: 10, right: 10, height: 3, borderRadius: 999, overflow: "hidden",
    backgroundColor: "rgba(255,255,255,0.25)",
  },
  inlineProgressFill: {
    height: "100%", backgroundColor: "#1E8E3E",
  },
  debugText: {
    position: "absolute",
    top: 0,
    left: 0,
    right: 0,
    bottom: 0,
    backgroundColor: "rgba(0,0,0,0.5)",
    color: "#fff",
    fontSize: 10,
    fontWeight: "bold",
    textAlign: "center",
    textShadowColor: "rgba(0,0,0,0.8)",
    textShadowOffset: { width: 0, height: 1 },
    textShadowRadius: 2,
  },
  debugButton: {
    position: "absolute",
    right: 12,
    bottom: 12,
    width: 50,
    height: 30,
    backgroundColor: "blue",
    borderRadius: 8,
    alignItems: "center",
    justifyContent: "center",
    borderWidth: 1,
    borderColor: "white",
    shadowColor: "black",
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.8,
    shadowRadius: 4,
    elevation: 10,
    zIndex: 1000,
  },
  debugButtonText: {
    color: "white",
    fontSize: 12,
    fontWeight: "bold",
    textShadowColor: "rgba(0,0,0,0.8)",
    textShadowOffset: { width: 0, height: 1 },
    textShadowRadius: 2,
  },
}); 