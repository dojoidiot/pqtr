import React from "react";
import { View, Text, StyleSheet } from "react-native";
import { colors, radius, space, text as T, shadow } from "../../theme/tokens";

export default function SectionCard({
  title, subtitle, children, footer,
}: {
  title: string;
  subtitle?: string;
  children: React.ReactNode;
  footer?: React.ReactNode;
}) {
  return (
    <View style={styles.wrap}>
      <Text style={T.h2}>{title}</Text>
      {!!subtitle && <Text style={[T.help, { marginTop: space.xs }]}>{subtitle}</Text>}
      <View style={styles.card}>{children}</View>
      {!!footer && <View style={styles.footer}>{footer}</View>}
    </View>
  );
}

const styles = StyleSheet.create({
  wrap: { marginHorizontal: space.lg, marginTop: space.lg },
  card: {
    backgroundColor: colors.surface, borderRadius: radius.lg, borderWidth: 1, borderColor: colors.stroke,
    marginTop: space.sm, overflow: "hidden", ...shadow.card,
  },
  footer: { marginTop: space.sm },
});
