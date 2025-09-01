export const colors = {
  bg: "#F8F6F1",
  surface: "#FFFFFF",
  stroke: "#E8E4DB",
  subtext: "#6E6A61",
  text: "#1F1D1A",
  brand: "#175E4C",
  brandSoft: "#E6F1EE",
  danger: "#C7352E",
  success: "#198754",
};

export const space = { xs: 4, sm: 8, md: 12, lg: 16, xl: 20, xxl: 24 };
export const radius = { sm: 8, md: 12, lg: 16, xl: 24, pill: 999 };
export const shadow = {
  card: { shadowColor: "#000", shadowOpacity: 0.06, shadowRadius: 8, shadowOffset: { width: 0, height: 3 }, elevation: 2 },
};
export const text = {
  title: { fontSize: 22, fontWeight: "800" as const, color: colors.text },
  h2: { fontSize: 16, fontWeight: "800" as const, color: colors.text },
  label: { fontSize: 14, fontWeight: "700" as const, color: colors.text },
  body: { fontSize: 14, color: colors.text },
  help: { fontSize: 12, color: colors.subtext },
  small: { fontSize: 12, fontWeight: "600" as const, color: colors.subtext },
};
