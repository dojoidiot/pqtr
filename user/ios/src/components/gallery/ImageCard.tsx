import React, { useRef } from "react";
import { View, Text, Image, StyleSheet, Pressable, Animated } from "react-native";
import { Ionicons } from "@expo/vector-icons";
import { ImageItem } from "../../types/images";
import { colors, radius, space, text as T, shadow } from "../../theme/tokens";

export default function ImageCard({
  item, width, selected, selecting, onTap, onLongPress,
}: {
  item: ImageItem;
  width: number;
  selected: boolean;
  selecting: boolean;
  onTap: () => void;            // single tap behavior decided by parent
  onLongPress: () => void;      // long press to enter select + select this item
}) {
  const ratio = item.height / item.width;
  const height = Math.max(140, Math.floor(width * ratio));

  return (
    <Pressable
      onPress={onTap}
      onLongPress={onLongPress}
      style={[styles.card, { width, height }]}
      accessibilityRole="imagebutton"
      accessibilityLabel={`${item.filename}, ${item.uploadedState}${selected ? ", selected" : ""}${item.favorite ? ", favorited" : ""}`}
    >
      <Image source={{ uri: item.uri }} style={[StyleSheet.absoluteFill, { borderRadius: radius.md }]} resizeMode="cover" />
      {/* status indicator */}
      <View style={styles.statusContainer}>
        <View style={[
          styles.dot,
          { backgroundColor:
            item.uploadedState === "synced" ? colors.success :
            item.uploadedState === "processing" ? colors.brand :
            item.uploadedState === "offline" ? colors.danger : colors.subtext }
        ]} />
        {item.uploadedState === "processing" && (
          <View style={styles.processingIndicator}>
            <Ionicons name="sync" size={10} color={colors.brand} />
          </View>
        )}
        {item.uploadedState === "offline" && (
          <View style={styles.offlineIndicator}>
            <Ionicons name="cloud-offline" size={10} color={colors.danger} />
          </View>
        )}
        {item.uploadedState === "processing" && item.uploadProgress !== undefined && (
          <View style={styles.progressIndicator}>
            <Text style={styles.progressText}>{Math.round(item.uploadProgress * 100)}%</Text>
          </View>
        )}
      </View>
      {/* favorite heart badge */}
      {item.favorite && (
        <View style={styles.fav}>
          <Ionicons name="heart" size={14} color="#fff" />
        </View>
      )}
      {/* selected check (only in selection mode) */}
      {selecting && selected ? (
        <View style={styles.check}>
          <Text style={{ color:"#fff", fontWeight:"800" }}>✓</Text>
        </View>
      ) : null}
      {/* filename overlay only when selected (and selecting) */}
      {selecting && selected ? (
        <View style={styles.caption}>
          <Text numberOfLines={1} style={{ color:"#fff", fontWeight:"800" }}>{item.filename}</Text>
        </View>
      ) : null}
      
      {/* Upload progress bar */}
      {item.uploadedState === "processing" && item.uploadProgress !== undefined && (
        <View style={styles.progressBarContainer}>
          <View style={styles.progressBar}>
            <View style={[styles.progressBarFill, { width: `${item.uploadProgress * 100}%` }]} />
          </View>
        </View>
      )}
    </Pressable>
  );
}

const styles = StyleSheet.create({
  card: {
    borderRadius: radius.md, overflow:"hidden", backgroundColor: colors.surface, ...shadow.card,
    marginBottom: space.xs,
  },
  statusContainer: { 
    position: "absolute", 
    top: space.md, 
    left: space.md, 
    flexDirection: "row", 
    alignItems: "center", 
    gap: 4 
  },
  dot: { 
    width: 8, 
    height: 8, 
    borderRadius: 4, 
    borderWidth: 1, 
    borderColor: "#fff" 
  },
  processingIndicator: {
    width: 16,
    height: 16,
    borderRadius: 8,
    backgroundColor: "rgba(255,255,255,0.9)",
    alignItems: "center",
    justifyContent: "center",
  },
  offlineIndicator: {
    width: 16,
    height: 16,
    borderRadius: 8,
    backgroundColor: "rgba(255,255,255,0.9)",
    alignItems: "center",
    justifyContent: "center",
  },
  progressIndicator: {
    paddingHorizontal: 6,
    paddingVertical: 2,
    borderRadius: 8,
    backgroundColor: "rgba(255,255,255,0.9)",
    alignItems: "center",
    justifyContent: "center",
    minWidth: 24,
  },
  progressText: {
    fontSize: 10,
    fontWeight: "700",
    color: colors.brand,
  },
  progressBarContainer: {
    position: "absolute",
    bottom: 0,
    left: 0,
    right: 0,
    paddingHorizontal: space.xs,
    paddingBottom: space.xs,
  },
  progressBar: {
    height: 3,
    backgroundColor: "rgba(255,255,255,0.3)",
    borderRadius: 2,
    overflow: "hidden",
  },
  progressBarFill: {
    height: "100%",
    backgroundColor: colors.brand,
    borderRadius: 2,
  },
  fav: {
    position: "absolute",
    bottom: 8,
    right: 8,
    width: 22,
    height: 22,
    borderRadius: 11,
    backgroundColor: "rgba(23,94,76,0.9)", // brand tinted
    alignItems: "center",
    justifyContent: "center",
  },
  check: { position:"absolute", top: space.md, right: space.md, width: 24, height: 24, borderRadius: 12, backgroundColor: colors.brand, alignItems:"center", justifyContent:"center" },
  caption: { position:"absolute", left: space.md, right: space.md, bottom: space.md, padding: space.md, backgroundColor: "rgba(0,0,0,0.35)", borderRadius: radius.md },
});
