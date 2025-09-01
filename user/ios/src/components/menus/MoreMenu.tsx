import React from "react";
import { Modal, View, Text, Pressable, StyleSheet } from "react-native";
import { colors, radius, space, text as T, shadow } from "../../theme/tokens";

export default function MoreMenu({
  visible, onClose, onAutomationSettings, onSelectAll, onDeselectAll, onChangeView, onSort,
}: {
  visible: boolean;
  onClose: () => void;
  onAutomationSettings: () => void;
  onSelectAll: () => void;
  onDeselectAll: () => void;
  onChangeView: () => void;
  onSort: () => void;
}) {
  return (
    <Modal visible={visible} transparent animationType="fade" onRequestClose={onClose}>
      <Pressable style={styles.backdrop} onPress={onClose} />
      <View style={styles.menu}>
        <Item label="Automation Settings" onPress={onAutomationSettings} />
        <Item label="Select All" onPress={onSelectAll} />
        <Item label="Deselect All" onPress={onDeselectAll} />
        <Item label="Change View (Grid/List)" onPress={onChangeView} />
        <Item label="Sort…" onPress={onSort} />
      </View>
    </Modal>
  );
}

function Item({ label, onPress }: { label: string; onPress: () => void }) {
  return (
    <Pressable onPress={onPress} style={styles.item}>
      <Text style={[T.body, { fontWeight:"800" }]}>{label}</Text>
    </Pressable>
  );
}

const styles = StyleSheet.create({
  backdrop: { flex:1, backgroundColor:"rgba(0,0,0,0.2)" },
  menu: {
    position:"absolute", right: space.lg, top: 80, backgroundColor: colors.surface,
    borderRadius: radius.lg, padding: space.sm, borderWidth:1, borderColor: colors.stroke,
    ...shadow.card,
  },
  item: { paddingHorizontal: space.lg, paddingVertical: space.md, borderRadius: radius.md, minHeight: 44 },
});
