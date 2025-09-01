import React from "react";
import { Modal, Pressable, View, Text, StyleSheet } from "react-native";
import { colors, radius, space, text as T } from "../../theme/tokens";

export default function ShareSheet({
  visible, onClose, selectedCount, onShare,
}: {
  visible: boolean;
  onClose: () => void;
  selectedCount: number;
  onShare: () => void;
}) {
  const empty = selectedCount === 0;
  return (
    <Modal visible={visible} animationType="slide" transparent onRequestClose={onClose}>
      <Pressable style={styles.backdrop} onPress={onClose} />
      <View style={styles.sheet}>
        <View style={styles.header}>
          <Text style={[T.body, { fontWeight:"800" }]}>Share</Text>
          <Pressable onPress={onClose}><Text style={[T.body, { color: colors.brand, fontWeight:"800" }]}>Close</Text></Pressable>
        </View>

        {empty ? (
          <View style={styles.tipBox}><Text style={[T.body, { textAlign:"center" }]}>Select images to share.</Text></View>
        ) : (
          <>
            <Pressable style={styles.action} onPress={onShare}><Text style={[T.body, { fontWeight:"800" }]}>System Share…</Text></Pressable>
            <Pressable style={styles.action} onPress={onShare}><Text style={[T.body, { fontWeight:"800" }]}>Save to Files</Text></Pressable>
            <Pressable style={styles.action} onPress={onShare}><Text style={[T.body, { fontWeight:"800" }]}>Copy Link</Text></Pressable>
          </>
        )}
      </View>
    </Modal>
  );
}

const styles = StyleSheet.create({
  backdrop: { flex:1, backgroundColor:"rgba(0,0,0,0.25)" },
  sheet: { position:"absolute", left:0, right:0, bottom:0, backgroundColor: colors.surface, borderTopLeftRadius: radius.xl, borderTopRightRadius: radius.xl, padding: space.xl, gap: space.lg },
  header: { flexDirection:"row", justifyContent:"space-between", alignItems:"center", marginBottom: space.md },
  tipBox: { padding: space.lg, backgroundColor:"#F3F1EC", borderRadius: radius.lg },
  action: { height: 52, borderRadius: radius.md, borderWidth:1, borderColor: colors.stroke, alignItems:"center", justifyContent:"center", backgroundColor:"#F7F5F0", marginBottom: space.md },
});
