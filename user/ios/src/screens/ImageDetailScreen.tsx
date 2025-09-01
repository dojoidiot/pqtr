import React, { useMemo, useRef, useState, useCallback } from "react";
import {
  SafeAreaView,
  View,
  Text,
  StyleSheet,
  Pressable,
  Image,
  ScrollView,
  Animated,
  LayoutAnimation,
  UIManager,
  Platform,
  Share,
  Alert,
  Dimensions,
} from "react-native";
import { Ionicons } from "@expo/vector-icons";
import { useImagesStore } from "../store/imagesStore";
import ActionBar, { ActionBarAction } from "../components/ActionBar";
import TagChips from "../components/TagChips";
import CollapsibleSection from "../components/CollapsibleSection";
import NotesEditor from "../components/NotesEditor";
import PresetSelectorModal from "../components/PresetSelectorModal";

// Android layout animation enable
if (Platform.OS === "android" && UIManager.setLayoutAnimationEnabledExperimental) {
  UIManager.setLayoutAnimationEnabledExperimental(true);
}

// ---- Types ----
type Status = "synced" | "syncing" | "needsSync" | "error";
type Asset = {
  id: string;
  uri: string;
  title: string;
  filename?: string;          // Add filename field for consistency
  createdAt: number;          // epoch ms
  status: Status;
  tags: string[];
  projectName?: string;
  notes?: string;
  favorite?: boolean;
  // Optional metadata
  exif?: {
    camera?: string;
    lens?: string;
    iso?: number;
    shutter?: string;
    focal?: string;
    aperture?: string;
    gps?: { lat: number; lon: number };
  };
};

// ---- TEMP MOCK (replace with store/route data) ----
function useAssetMock(id: string): Asset {
  return useMemo(() => ({
    id,
    uri: "https://images.unsplash.com/photo-1543349689-9a4d426bee8f?w=2000",
    title: "Paris Eiffel Tower",
    filename: "IMG_001.jpg", // Add filename for consistency
    createdAt: Date.now() - 1000 * 60 * 42,
    status: "synced",
    projectName: "City Nights",
    tags: ["Formula 1", "Press", "VIP"],
    notes: "Shot from the bridge. Ask client if they want warmer grade.",
    favorite: false,
    exif: {
      camera: "Canon EOS R5",
      lens: "RF 70–200mm f/2.8",
      iso: 200,
      shutter: "1/250s",
      focal: "70mm",
      aperture: "f/2.8",
      gps: { lat: 48.8584, lon: 2.2945 },
    },
  }), [id]);
}

// ---- Helpers ----
function formatDateTime(ts: number) {
  const d = new Date(ts);
  const date = d.toLocaleDateString(undefined, { year: "numeric", month: "short", day: "numeric" });
  const time = d.toLocaleTimeString(undefined, { hour: "2-digit", minute: "2-digit" });
  return { date, time };
}

export default function ImageDetailScreen({ navigation, route }: any) {
  const { id = "asset-1" } = route?.params ?? {};
  const asset = useAssetMock(id);
  const { date, time } = formatDateTime(asset.createdAt);

  // Get image from store and favorite functionality
  const storeImage = useImagesStore((s) => s.getById(route.params?.projectId || 'default', id));
  const toggleFavorite = useImagesStore((s) => s.toggleFavorite);
  
  // Use store image if available, otherwise fall back to mock
  const image = storeImage || asset;
  const isFavorited = image.favorite || false;
  
  // Preset selector modal state
  const [presetModalVisible, setPresetModalVisible] = useState(false);
  
  // Get the display name - prefer filename from store, fall back to filename from mock, then title
  const displayName = (storeImage?.filename || image.filename || image.title || 'Untitled Image');

  // UI state
  const [tags, setTags] = useState(image.tags);
  const [notes, setNotes] = useState(image.notes ?? "");
  const [metaOverlay, setMetaOverlay] = useState(true);

  const fade = useRef(new Animated.Value(1)).current;

  const toggleOverlay = useCallback(() => {
    LayoutAnimation.configureNext(LayoutAnimation.Presets.easeInEaseOut);
    setMetaOverlay(v => !v);
    Animated.timing(fade, { toValue: metaOverlay ? 0 : 1, duration: 160, useNativeDriver: true }).start();
  }, [fade, metaOverlay]);

  // Double-tap detection for favorite
  const lastTap = useRef<number>(0);
  const onImagePress = useCallback(() => {
    const now = Date.now();
    if (now - lastTap.current < 300) {
      // Double-tap detected - toggle favorite
      if (storeImage) {
        toggleFavorite(route.params?.projectId || 'default', id);
      }
    } else {
      // Single-tap - toggle overlay
      toggleOverlay();
    }
    lastTap.current = now;
  }, [id, storeImage, toggleFavorite, toggleOverlay, route.params?.projectId]);

  // Handle favorite toggle
  const handleToggleFavorite = useCallback(() => {
    if (storeImage) {
      toggleFavorite(route.params?.projectId || 'default', id);
    }
  }, [id, storeImage, toggleFavorite, route.params?.projectId]);

  // Status color
  const statusColor =
    image.status === "synced" ? "#2D9C5A"
    : image.status === "syncing" ? "#F59E0B"
    : image.status === "error" ? "#DC2626"
    : "#A8A29E";

  // Actions
  const actions: ActionBarAction[] = [
    {
      key: "preset",
      icon: "color-palette-outline",
      label: "Preset",
      onPress: () => setPresetModalVisible(true),
    },
    {
      key: "tags",
      icon: "pricetags-outline",
      label: "Tags",
      onPress: () => {
        // scroll to tags section if you add a ref (optional)
      },
    },
    {
      key: "share",
      icon: "share-outline",
      label: "Share",
      onPress: async () => {
        try {
          await Share.share({
            title: image.title,
            message: `${image.title} • ${time} ${date}\n${image.uri}`,
            url: image.uri, // iOS uses this field
          });
        } catch (e) {}
      },
    },
    {
      key: "delete",
      icon: "trash-outline",
      danger: true,
      label: "Delete",
      onPress: () => {
        Alert.alert(
          "Delete Photo?",
          "This removes the photo from the project. You can't undo this.",
          [
            { text: "Cancel", style: "cancel" },
            { text: "Delete", style: "destructive", onPress: () => {/* delete from store & goBack() */} },
          ]
        );
      },
    },
  ];

  // Tag handlers
  const addTag = (label: string) => setTags((prev: string[]) => (prev.includes(label) ? prev : [...prev, label]));
  const removeTag = (label: string) => setTags((prev: string[]) => prev.filter((t: string) => t !== label));

  // Notes save handler (called by NotesEditor after debounce)
  const handleSaveNotes = useCallback((value: string) => {
    setNotes(value);
    // persist to store/network here
  }, []);

  return (
    <SafeAreaView style={styles.safe}>
      {/* Top bar */}
      <View style={styles.topBar}>
        <Pressable
          hitSlop={10}
          onPress={() => navigation.goBack()}
          accessibilityRole="button"
          accessibilityLabel="Go back"
        >
          <Ionicons name="chevron-back" size={22} color="#111827" />
        </Pressable>
        <Text style={styles.navTitle} numberOfLines={1}>{displayName}</Text>
        <Pressable
          hitSlop={10}
          onPress={handleToggleFavorite}
          accessibilityRole="button"
          accessibilityLabel={isFavorited ? "Unfavorite" : "Favorite"}
        >
          <Ionicons
            name={isFavorited ? "heart" : "heart-outline"}
            size={24}
            color={isFavorited ? "#FF5A5F" : "#111827"}
          />
        </Pressable>
      </View>

      <ScrollView
        contentContainerStyle={{ paddingBottom: 24 }}
        showsVerticalScrollIndicator={false}
      >
        {/* Image hero */}
        <Pressable onPress={onImagePress} style={styles.imageWrap} accessibilityLabel="Photo. Double tap to toggle overlay, double tap to favorite.">
          <Image source={{ uri: image.uri }} style={styles.image} resizeMode="cover" />
          {/* tiny status dot */}
          <View style={styles.dotWrap}><View style={[styles.dot, { backgroundColor: statusColor }]} /></View>

          {/* metadata overlay toggle */}
          {metaOverlay && (
            <Animated.View style={[styles.metaOverlay, { opacity: fade }]}>
              <Text style={styles.metaOverlayTitle} numberOfLines={1}>{displayName}</Text>
              <Text style={styles.metaOverlaySub} numberOfLines={1}>{time} • {date}</Text>
              {!!image.projectName && <Text style={styles.metaOverlaySub} numberOfLines={1}>{image.projectName}</Text>}
              <Text style={styles.metaHint}>Tap image to toggle overlay • Double-tap to favorite</Text>
            </Animated.View>
          )}
        </Pressable>

        {/* Title block */}
        <View style={styles.titleBlock}>
          <Text style={styles.h2} numberOfLines={2}>{displayName}</Text>
          <Text style={styles.subtle}>{time} • {date}</Text>
          {!!image.projectName && <Text style={[styles.subtle, { marginTop: 2 }]}>{image.projectName}</Text>}
        </View>

        {/* Primary actions */}
        <ActionBar actions={actions} style={{ marginHorizontal: 16 }} />

        {/* Tags */}
        <View style={{ marginTop: 16 }}>
          <TagChips
            title="Tags"
            tags={tags}
            onAdd={addTag}
            onRemove={removeTag}
            style={{ marginHorizontal: 16 }}
          />
        </View>

        {/* Notes (editable, autosave) */}
        <CollapsibleSection
          title="Notes"
          style={{ marginTop: 12, marginHorizontal: 16 }}
          initiallyOpen={!!notes}
        >
          <NotesEditor
            value={notes}
            onChangeDebounced={handleSaveNotes}
            maxChars={1000}
            placeholder="Add a note for this photo (client context, tech setup, approvals, etc.)"
          />
        </CollapsibleSection>

        {/* Technical metadata */}
        {image.exif && (
          <CollapsibleSection
            title="Technical"
            style={{ marginTop: 12, marginHorizontal: 16 }}
            initiallyOpen={false}
          >
            <View style={styles.metaTable}>
              {image.exif.camera && <MetaRow label="Camera" value={image.exif.camera} />}
              {image.exif.lens && <MetaRow label="Lens" value={image.exif.lens} />}
              {image.exif.aperture && <MetaRow label="Aperture" value={image.exif.aperture} />}
              {image.exif.shutter && <MetaRow label="Shutter" value={image.exif.shutter} />}
              {image.exif.iso && <MetaRow label="ISO" value={String(image.exif.iso)} />}
              {image.exif.focal && <MetaRow label="Focal Length" value={image.exif.focal} />}
              {!!image.exif.gps && (
                <MetaRow label="Location" value={`${image.exif.gps.lat.toFixed(4)}, ${image.exif.gps.lon.toFixed(4)}`} />
              )}
            </View>
          </CollapsibleSection>
        )}
      </ScrollView>
      
      {/* Preset Selector Modal */}
      <PresetSelectorModal
        isVisible={presetModalVisible}
        onClose={() => setPresetModalVisible(false)}
        onSelectPreset={(preset) => {
          console.log('Selected preset:', preset.name, 'for image:', image.id);
          setPresetModalVisible(false);
          // TODO: Apply preset to image
        }}
      />
    </SafeAreaView>
  );
}

function MetaRow({ label, value }: { label: string; value: string }) {
  return (
    <View style={styles.metaRow}>
      <Text style={styles.metaLabel}>{label}</Text>
      <Text style={styles.metaValue}>{value}</Text>
    </View>
  );
}

const R = 16;

const styles = StyleSheet.create({
  safe: { flex: 1, backgroundColor: "#F7F4EF" },

  topBar: {
    height: 44,
    paddingHorizontal: 12,
    alignItems: "center",
    flexDirection: "row",
    gap: 12,
  },
  navTitle: { flex: 1, textAlign: "center", fontWeight: "800", color: "#111827" },

  imageWrap: {
    margin: 16,
    borderRadius: R,
    overflow: "hidden",
    backgroundColor: "#E7E1D7",
  },
  image: { width: "100%", height: 380 },

  dotWrap: { position: "absolute", top: 10, left: 10 },
  dot: { width: 10, height: 10, borderRadius: 999, borderWidth: 1, borderColor: "rgba(255,255,255,0.9)" },

  metaOverlay: {
    position: "absolute",
    left: 12, right: 12, bottom: 12,
    backgroundColor: "rgba(0,0,0,0.35)",
    paddingHorizontal: 12, paddingVertical: 10,
    borderRadius: 12,
  },
  metaOverlayTitle: { color: "#fff", fontWeight: "800", fontSize: 16 },
  metaOverlaySub: { color: "rgba(255,255,255,0.95)", fontSize: 12, marginTop: 2 },
  metaHint: { color: "rgba(255,255,255,0.85)", fontSize: 11, marginTop: 4 },

  titleBlock: { paddingHorizontal: 16, marginTop: 4 },
  h2: { fontSize: 22, fontWeight: "800", color: "#111827" },
  subtle: { color: "#6B7280", marginTop: 4, fontWeight: "600" },

  metaTable: { paddingVertical: 4 },
  metaRow: { flexDirection: "row", paddingVertical: 8, borderBottomWidth: StyleSheet.hairlineWidth, borderColor: "#ECE7DF" },
  metaLabel: { width: 130, color: "#6B7280", fontWeight: "700" },
  metaValue: { flex: 1, color: "#111827", fontWeight: "700" },
});
