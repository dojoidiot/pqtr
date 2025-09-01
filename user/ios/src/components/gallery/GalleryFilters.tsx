import React from "react";
import { View, Text, StyleSheet, Pressable, TextInput } from "react-native";
import { Ionicons } from "@expo/vector-icons";
import Slider from "@react-native-community/slider";
import { colors, radius, space, text as T } from "../../theme/tokens";

type FilterTab = "all"|"favorites"|"processed"|"unprocessed";

export default function GalleryFilters({
  filter, onChangeFilter, query, onChangeQuery, onUpload,
  gridWidth, onChangeGridWidth,
}: {
  filter: FilterTab;
  onChangeFilter: (t: FilterTab) => void;
  query: string;
  onChangeQuery: (q: string) => void;
  onUpload: () => void;
  gridWidth: number;                   // NEW
  onChangeGridWidth: (w: number) => void; // NEW
}) {
  // map actual width (120..260) to slider (0..1) & back
  const MIN = 120, MAX = 260;
  const toVal = (w: number) => (w - MIN) / (MAX - MIN);
  const fromVal = (v: number) => Math.round(MIN + v * (MAX - MIN));

  return (
    <View style={styles.wrap}>
      <View style={styles.segment}>
        {(["all","favorites","processed","unprocessed"] as FilterTab[]).map((t) => {
          const active = filter === t;
          const label = t === "all" ? "All" : t === "favorites" ? "Favorites" : t === "processed" ? "Processed" : "Unprocessed";
          return (
            <Pressable key={t} onPress={() => onChangeFilter(t)} style={[styles.segBtn, active && styles.segBtnActive]}>
              <Text style={[styles.segTxt, active && styles.segTxtActive]}>{label}</Text>
            </Pressable>
          );
        })}
      </View>

      <View style={styles.searchRow}>
        <View style={styles.search}>
          <Ionicons name="search" size={16} color={colors.subtext} />
          <TextInput
            placeholder="Search photos..."
            placeholderTextColor={colors.subtext}
            value={query}
            onChangeText={onChangeQuery}
            style={styles.input}
          />
        </View>
        <Pressable onPress={onUpload} style={styles.iconBtn}>
          <Ionicons name="add" size={20} color={colors.text} />
        </Pressable>
      </View>

      {/* NEW: Grid size control */}
      <View style={styles.sizeRow} accessible accessibilityLabel="Grid size">
        <View style={{ flexDirection:"row", alignItems:"center", justifyContent:"space-between" }}>
          <Text style={[T.small, { color: colors.subtext }]}>Thumb Size</Text>
          {/* small S / L markers */}
          <View style={{ flexDirection:"row", gap: 10, alignItems:"center" }}>
            <Text style={[T.small, { color: colors.subtext }]}>S</Text>
            <View style={{ width: 1, height: 10, backgroundColor: colors.stroke }} />
            <Text style={[T.small, { color: colors.subtext }]}>L</Text>
          </View>
        </View>
        <Slider
          style={{ width: "100%", height: 36 }}
          minimumValue={0}
          maximumValue={1}
          value={toVal(gridWidth)}
          onValueChange={(v) => onChangeGridWidth(fromVal(v))}
          minimumTrackTintColor={colors.brand}
          maximumTrackTintColor="#D9D5CC"
          thumbTintColor={colors.brand}
        />
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  wrap: { paddingHorizontal: space.lg, gap: space.lg, marginTop: space.md, marginBottom: space.xl },
  segment: { backgroundColor:"#ECE9E2", borderRadius: radius.lg, padding: 4, flexDirection:"row", gap: 4 },
  segBtn: { paddingVertical: space.md, paddingHorizontal: space.lg, borderRadius: radius.md },
  segBtnActive: { backgroundColor: colors.surface },
  segTxt: { ...T.small },
  segTxtActive: { color: colors.text },
  searchRow: { flexDirection:"row", gap: space.md, alignItems:"center" },
  search: {
    flex:1, flexDirection:"row", alignItems:"center", gap: 8,
    backgroundColor: colors.surface, borderRadius: radius.lg, paddingHorizontal: 12, height: 44,
    borderWidth: 1, borderColor: colors.stroke,
  },
  input: { flex:1, ...T.body },
  iconBtn: {
    height: 44, width: 44, borderRadius: radius.lg, backgroundColor: colors.surface,
    alignItems:"center", justifyContent:"center", borderWidth: 1, borderColor: colors.stroke,
  },
  sizeRow: {
    backgroundColor: colors.surface,
    borderRadius: radius.lg,
    borderWidth: 1,
    borderColor: colors.stroke,
    paddingHorizontal: space.lg,
    paddingVertical: space.md,
    gap: space.sm,
  },
});
