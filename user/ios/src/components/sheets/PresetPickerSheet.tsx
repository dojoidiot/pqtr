import React from "react";
import { Modal, View, Text, StyleSheet, Pressable, FlatList } from "react-native";
import { colors, radius, space, text as T } from "../../theme/tokens";

const PRESETS = [
  { id: "p1", name: "Cinematic Contrast" },
  { id: "p2", name: "BW 01" },
  { id: "p3", name: "Vivid Sports" },
];

export default function PresetPickerSheet({
  visible, onClose, onApply, selectedCount,
}: {
  visible: boolean;
  onClose: () => void;
  onApply: (presetId: string) => void;
  selectedCount: number;
}) {
  const empty = selectedCount === 0;
  return (
    <Modal visible={visible} animationType="slide" transparent onRequestClose={onClose}>
      <Pressable style={styles.backdrop} onPress={onClose} />
      <View style={styles.sheet}>
        <View style={styles.header}>
          <Text style={[T.body, { fontWeight:"800" }]}>Apply Preset</Text>
          <Pressable onPress={onClose}><Text style={[T.body, { color: colors.brand, fontWeight:"800" }]}>Close</Text></Pressable>
        </View>

        {empty ? (
          <View style={styles.tipBox}>
            <Text style={[T.body, { textAlign:"center" }]}>Select images to apply a preset.</Text>
          </View>
        ) : (
          <FlatList
            data={PRESETS}
            keyExtractor={(i) => i.id}
            ItemSeparatorComponent={() => <View style={{ height: 8 }} />}
            contentContainerStyle={{ paddingBottom: 16 }}
            renderItem={({ item }) => (
              <Pressable onPress={() => onApply(item.id)} style={styles.presetBtn}>
                <Text style={[T.body, { fontWeight:"800" }]}>{item.name}</Text>
              </Pressable>
            )}
          />
        )}
      </View>
    </Modal>
  );
}

const styles = StyleSheet.create({
  backdrop: { flex:1, backgroundColor: "rgba(0,0,0,0.25)" },
  sheet: { position:"absolute", left:0, right:0, bottom:0, backgroundColor: colors.surface, borderTopLeftRadius: radius.xl, borderTopRightRadius: radius.xl, padding: space.xl, gap: space.lg },
  header: { flexDirection:"row", justifyContent:"space-between", alignItems:"center", marginBottom: space.md },
  tipBox: { padding: space.lg, backgroundColor:"#F3F1EC", borderRadius: radius.lg },
  presetBtn: { height: 52, borderRadius: radius.md, borderWidth:1, borderColor: colors.stroke, alignItems:"center", justifyContent:"center", backgroundColor:"#F7F5F0" },
});
