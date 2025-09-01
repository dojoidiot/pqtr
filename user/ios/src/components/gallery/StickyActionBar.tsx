import React from "react";
import { View, Text, StyleSheet, Pressable } from "react-native";
import { Ionicons } from "@expo/vector-icons";
import { colors, radius, space, text as T, shadow } from "../../theme/tokens";

export default function StickyActionBar({
  selectedCount, selecting, onPreset, onShare, onClear, onMore, onDone,
}: {
  selectedCount: number;
  selecting: boolean;
  onPreset: () => void;
  onShare: () => void;
  onClear: () => void;
  onMore: () => void;
  onDone: () => void;
}) {
  const selected = selectedCount > 0;
  return (
    <View style={styles.wrap} pointerEvents="box-none">
      <View style={styles.bar}>
        {selecting ? (
          <>
            <Text style={[T.body, { fontWeight:"800", flex:1 }]}>{selectedCount} selected</Text>
            <Action icon="color-palette-outline" label="Apply" onPress={onPreset} />
            <Action icon="share-outline" label="Share" onPress={onShare} />
            <Action icon="checkmark" label="Done" onPress={onDone} />
          </>
        ) : (
          <>
            <Action icon="color-palette-outline" label="Preset" onPress={onPreset} />
            <Action icon="share-outline" label="Share" onPress={onShare} />
            <Action icon="ellipsis-horizontal" label="More" onPress={onMore} />
          </>
        )}
      </View>
    </View>
  );
}

function Action({ icon, label, onPress }: { icon: any; label: string; onPress: () => void }) {
  return (
    <Pressable onPress={onPress} style={styles.btn} accessibilityRole="button">
      <Ionicons name={icon} size={17} color={colors.text} />
      <Text style={[T.body, { fontWeight:"800" }]}>{label}</Text>
    </Pressable>
  );
}

const styles = StyleSheet.create({
  wrap: { position:"absolute", left: 0, right: 0, bottom: space.xl, alignItems:"center" },
  bar: {
    flexDirection:"row", alignItems:"center", gap: space.md,
    backgroundColor: colors.surface, borderRadius: radius.lg, paddingVertical: space.md, paddingHorizontal: space.lg,
    borderWidth: 1, borderColor: colors.stroke, ...shadow.card,
    width: "92%",
  },
  btn: { flex:1, height: 44, borderRadius: radius.md, backgroundColor: "#F3F1EC", alignItems:"center", justifyContent:"center", flexDirection:"row", gap: 8 },
});
