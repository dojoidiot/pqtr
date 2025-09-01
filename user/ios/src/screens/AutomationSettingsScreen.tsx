// screens/AutomationSettingsScreen.tsx
import React from "react";
import { SafeAreaView, View, Text, ScrollView, Pressable, StyleSheet, Alert } from "react-native";
import { useRoute, RouteProp, useNavigation } from "@react-navigation/native";
import { Ionicons } from "@expo/vector-icons";
import { colors, radius, space, text as T } from "../theme/tokens";
import { useProjectAutomation, ProjectAutomation, Aspect, ExportFormat, WatermarkPosition } from "../store/projectAutomationStore";
import SectionCard from "../components/settings/SectionCard";
import { RowTitle, RowToggle, RowStepper, RowChipGroup, RowPicker, RowDropdown } from "../components/settings/rows/Row";

type Rt = RouteProp<Record<"AutomationSettings",{ projectId: string }>, "AutomationSettings">;

const ASPECT_OPTS: Aspect[] = ["1:1","4:5","3:2","16:9","9:16"];
const STORAGE_DEMO = ["Archive","Client Delivery","Newsroom","Social","Print"];

export default function AutomationSettingsScreen() {
  const route = useRoute<Rt>();
  const nav = useNavigation();
  const { projectId } = route.params;

  const store = useProjectAutomation();
  React.useEffect(() => { store.seed(projectId); }, [projectId]);

  const src = store.get(projectId);
  const [draft, setDraft] = React.useState<ProjectAutomation | null>(src ?? null);
  React.useEffect(() => { if (src) setDraft(src); }, [src?.updatedAt]);

  const dirty = React.useMemo(() => JSON.stringify(src) !== JSON.stringify(draft), [src, draft]);
  if (!draft) return null;

  const patch = (p: Partial<ProjectAutomation>) => setDraft((d) => ({ ...(d as ProjectAutomation), ...p }));



  const toggleAspect = (a: Aspect) => {
    const v = new Set(draft.aspects);
    v.has(a) ? v.delete(a) : v.add(a);
    patch({ aspects: Array.from(v) });
  };

  const toggleChannel = (name: string) => {
    const v = new Set(draft.storageChannels);
    v.has(name) ? v.delete(name) : v.add(name);
    patch({ storageChannels: Array.from(v) });
  };

  const onSave = () => {
    store.replace(projectId, { ...draft, projectId });
    nav.goBack();
  };
  const onRevert = () => setDraft(src!);

  return (
    <SafeAreaView style={{ flex:1, backgroundColor: colors.bg }}>
      {/* Header */}
      <View style={styles.header}>
        <Pressable onPress={() => nav.goBack()} style={styles.headerBtn}>
          <Ionicons name="chevron-back" size={20} color={colors.text} />
          <Text style={[T.body, { fontWeight:"800" }]}>Back</Text>
        </Pressable>
        <Text style={[T.title]}>Automation Settings</Text>
        <View style={{ width: 60 }} />
      </View>

      <ScrollView contentContainerStyle={{ paddingBottom: 120 }}>
        {/* Processing */}
        <SectionCard
          title="Processing"
          subtitle="Define default preset behavior for new and existing photos."
        >
          <RowTitle title="Default preset" help="Applied automatically to new images. Long-press to chain presets in the picker." />
          <RowPicker
            title="Preset"
            value={draft.defaultPresetName ?? "None"}
            onPress={() => Alert.alert("Preset picker (MVP)", "Open your preset picker here")}
          />
          <RowToggle
            title="Auto-apply to new uploads"
            value={draft.autoApplyNew}
            onValueChange={(v) => patch({ autoApplyNew: v })}
          />
          <RowToggle
            title="Retro-apply to existing files"
            help="Runs once in the background after saving."
            value={draft.retroApplyExisting}
            onValueChange={(v) => patch({ retroApplyExisting: v })}
          />
        </SectionCard>

        {/* Output */}
        <SectionCard title="File Output" subtitle="Control export format and filename rules.">
          <RowDropdown 
            title="Export format" 
            value={draft.exportFormat} 
            options={["JPEG", "TIFF", "PNG"]}
            onValueChange={(format) => patch({ exportFormat: format })}
          />
          <RowTitle title="File naming rule" help="Tokens: {project} {date} {counter} {tag} {original}" />
          {/* Simple inline editor; replace with text input screen if needed */}
          <View style={styles.inlineInput}><Text style={T.body}>{draft.fileRule}</Text></View>
          <RowStepper
            title="Counter starts at"
            value={draft.counterStart}
            onChange={(n) => patch({ counterStart: Math.max(1,n) })}
            min={1}
          />
          <RowToggle
            title="Include original filename"
            value={draft.includeOriginalName}
            onValueChange={(v) => patch({ includeOriginalName: v })}
          />
        </SectionCard>

        {/* Watermark */}
        <SectionCard title="Watermarking" subtitle="Applied on export (not on originals).">
          <RowToggle
            title="Enable watermark"
            value={draft.enableWatermark}
            onValueChange={(v) => patch({ enableWatermark: v })}
          />
          {draft.enableWatermark && (
            <>
              <RowTitle title="Watermark text" />
              <View style={styles.inlineInput}><Text style={T.body}>{draft.watermarkText}</Text></View>
              <RowDropdown 
                title="Position" 
                value={draft.wmPosition} 
                options={["Top-Left", "Top-Right", "Bottom-Left", "Bottom-Right", "Center"]}
                onValueChange={(position) => patch({ wmPosition: position })}
              />
              <RowStepper title="Opacity" value={draft.wmOpacity} onChange={(n)=>patch({ wmOpacity: Math.max(0, Math.min(100,n)) })} min={0} max={100} />
            </>
          )}
        </SectionCard>

        {/* Tagging & QC */}
        <SectionCard title="Tagging & Quality" subtitle="Automate tags and basic quality thresholds.">
          <RowTitle title="Default tags (comma separated)" />
          <View style={styles.inlineInput}>
            <Text style={[T.body, { color: draft.defaultTags ? colors.text : colors.subtext }]}>
              {draft.defaultTags || "F1, Pit lane, Mercedes"}
            </Text>
          </View>
          <RowToggle
            title="Smart auto-tagging"
            help="AI suggests tags on ingest. You can review before publish."
            value={draft.smartAutoTagging}
            onValueChange={(v)=>patch({ smartAutoTagging: v })}
          />
          <RowStepper
            title="Minimum long-edge resolution (px)"
            value={draft.minLongEdgePx}
            onChange={(n)=>patch({ minLongEdgePx: Math.max(320, n) })}
            min={320}
          />
          <RowStepper
            title="Min exposure quality (0–100)"
            value={draft.minExposureQuality}
            onChange={(n)=>patch({ minExposureQuality: Math.max(0, Math.min(100,n)) })}
            min={0}
            max={100}
          />
          <RowToggle
            title="Auto-flag low quality"
            value={draft.autoFlagLowQuality}
            onValueChange={(v)=>patch({ autoFlagLowQuality: v })}
          />
        </SectionCard>

        {/* Aspect Ratios */}
        <SectionCard title="Aspect Ratios & Crops" subtitle="Choose multiple outputs for social/web/newsroom delivery.">
          <RowChipGroup title="Outputs" options={ASPECT_OPTS} value={draft.aspects} onToggle={(a)=>toggleAspect(a)} />
        </SectionCard>

        {/* Distribution */}
        <SectionCard title="Distribution" subtitle="Route images to channels after processing.">
          <RowChipGroup title="PQTR Storage Channels" options={STORAGE_DEMO} value={draft.storageChannels as any} onToggle={toggleChannel} />
          <RowToggle
            title="AI distribution"
            help="Future: route to the right people/places based on content."
            value={draft.aiDistribution}
            onValueChange={(v)=>patch({ aiDistribution: v })}
          />
          <RowTitle title="AI notes" />
          <View style={styles.inlineInput}><Text style={T.body}>{draft.aiNotes || "e.g., prioritise sponsor logos and car close-ups."}</Text></View>
        </SectionCard>

        {/* Scheduling */}
        <SectionCard title="Scheduling" subtitle="(MVP demo) Replace with actual picker later.">
          <RowToggle
            title="Export schedule"
            value={draft.exportSchedulingOn}
            onValueChange={(v)=>patch({ exportSchedulingOn: v })}
            right={<Text style={[T.body, { color: colors.subtext, marginRight: 8 }]}>{draft.exportSchedulingOn ? "On" : "Off"}</Text>}
          />
        </SectionCard>
      </ScrollView>

      {/* Sticky footer */}
      <View style={styles.footer}>
        <Pressable
          onPress={onRevert}
          disabled={!dirty}
          style={[styles.ghostBtn, !dirty && { opacity: 0.5 }]}
        >
          <Text style={[T.label, { color: colors.text }]}>Revert</Text>
        </Pressable>
        <Pressable
          onPress={onSave}
          disabled={!dirty}
          style={[styles.primaryBtn, !dirty && { opacity: 0.5 }]}
        >
          <Text style={[T.label, { color: "#fff" }]}>Save changes</Text>
        </Pressable>
      </View>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  header: {
    height: 52, flexDirection:"row", alignItems:"center", justifyContent:"space-between",
    paddingHorizontal: space.lg, backgroundColor: colors.bg,
  },
  headerBtn: { flexDirection:"row", alignItems:"center", gap: 4 },
  inlineInput: {
    marginHorizontal: space.lg, marginTop: space.sm, marginBottom: space.md,
    height: 44, borderRadius: radius.md, borderWidth: 1, borderColor: colors.stroke,
    backgroundColor: colors.surface, justifyContent:"center", paddingHorizontal: 12,
  },
  footer: {
    position:"absolute", left:0, right:0, bottom:0, padding: space.lg,
    backgroundColor: colors.bg, flexDirection:"row", gap: space.md,
    borderTopWidth: 1, borderTopColor: colors.stroke,
  },
  primaryBtn: {
    flex:1, height: 48, borderRadius: radius.lg, alignItems:"center", justifyContent:"center",
    backgroundColor: colors.brand,
  },
  ghostBtn: {
    width: 120, height: 48, borderRadius: radius.lg, alignItems:"center", justifyContent:"center",
    borderWidth: 1, borderColor: colors.stroke, backgroundColor: colors.surface,
  },
});
