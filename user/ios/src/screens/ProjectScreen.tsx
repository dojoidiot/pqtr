import React, { useState, useCallback, useMemo, useEffect } from 'react';
import {
  View,
  Text,
  StyleSheet,
  SafeAreaView,
  TouchableOpacity,
  Alert,
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import { useNavigation, useRoute } from '@react-navigation/native';
import * as Haptics from 'expo-haptics';
import AsyncStorage from '@react-native-async-storage/async-storage';
import { colors, radius, space, text as T, shadow } from '../theme/tokens';
import { ImageItem } from '../types/images';
import { useSelection } from '../store/selectionStore';
import { useImagesStore } from '../store/imagesStore';
import { usePhotos, useProject } from '../hooks/useDatabase';
import { GalleryFilters, GalleryGrid, StickyActionBar } from '../components/gallery';
import { PresetPickerSheet, ShareSheet } from '../components/sheets';
import { MoreMenu } from '../components/menus';
import { toast } from '../utils/toast';

type FilterTab = "all"|"favorites"|"processed"|"unprocessed";

export default function ProjectScreen() {
  const navigation = useNavigation<any>();
  const route = useRoute<any>();
  const projectId = route?.params?.projectId ?? 'project-1';
  const projectTitle = route?.params?.projectTitle ?? 'Untitled Project';
  
  // State
  const [filter, setFilter] = useState<FilterTab>('all');
  const [searchQuery, setSearchQuery] = useState('');
  const [targetCardWidth, setTargetCardWidth] = useState(180); // range: 120..260 (phones)
  const [presetSheetVisible, setPresetSheetVisible] = useState(false);
  const [shareSheetVisible, setShareSheetVisible] = useState(false);
  const [moreMenuVisible, setMoreMenuVisible] = useState(false);
  
  // Selection store
  const sel = useSelection();
  
  // Database hooks
  const { photos: dbPhotos, loading: photosLoading, error: photosError } = usePhotos(projectId);
  const { project: projectData, loading: projectLoading, error: projectError } = useProject(projectId);
  
  // Images store - use store methods directly to avoid selector recreation
  const store = useImagesStore();
  const storeItems = store.getByProject(projectId);
  const seed = store.seed;
  const loadFromStorage = store.loadFromStorage;
  
  // Debug logging
  useEffect(() => {
    console.log('ProjectScreen - Route params:', route?.params);
    console.log('ProjectScreen - Project ID:', projectId);
    console.log('ProjectScreen - Project Title:', projectTitle);
  }, [route?.params, projectId, projectTitle]);
  
  // Log when store items change
  useEffect(() => {
    console.log('ProjectScreen - Store items updated for project:', projectId, 'Count:', storeItems.length);
  }, [projectId, storeItems.length]);
  
  // Load and save grid width preference
  useEffect(() => {
    AsyncStorage.getItem("gridWidth").then(v => {
      if (v) setTargetCardWidth(Number(v));
    });
  }, []);
  
  useEffect(() => {
    AsyncStorage.setItem("gridWidth", String(targetCardWidth));
  }, [targetCardWidth]);
  
  // Load images from storage on mount
  useEffect(() => {
    loadFromStorage();
  }, [loadFromStorage]);
  
  // Seed store with project-specific photos from database
  useEffect(() => {
    if (dbPhotos && dbPhotos.length > 0 && storeItems.length === 0) {
      // Convert database photos to ImageItem format and seed the store
      const convertedPhotos: ImageItem[] = dbPhotos.map((photo, index) => ({
        id: photo.id,
        uri: photo.uri,
        width: 400, // Default width since database doesn't have dimensions
        height: 300, // Default height since database doesn't have dimensions
        filename: photo.title || `Photo ${index + 1}`,
        uploadedState: photo.uploadStatus === 'synced' ? 'synced' : 
                      photo.uploadStatus === 'uploading' ? 'processing' : 
                      photo.uploadStatus === 'error' ? 'offline' : 'processing',
        uploadProgress: photo.uploadProgress,
        processed: photo.status !== 'raw',
        favorite: false, // Default to false, can be managed separately
        tags: photo.tags || []
      }));
      
      console.log('ProjectScreen - Seeding with', convertedPhotos.length, 'photos from database for project:', projectId);
      seed(projectId, convertedPhotos);
    }
  }, [projectId, dbPhotos, storeItems.length, seed]);
  
  // Use project data from database when available
  const projectName = projectData?.name || projectTitle;
  const projectSubtitle = projectData?.description || "Project photography";
  const projectTags = projectData?.tags || ["project", "photos"];
  
  // Filter images based on current filter and search
  const filteredImages = useMemo(() => {
    let filtered = storeItems;
    
    // Apply filter
    switch (filter) {
      case 'favorites':
        filtered = filtered.filter(img => img.favorite);
        break;
      case 'processed':
        filtered = filtered.filter(img => img.processed);
        break;
      case 'unprocessed':
        filtered = filtered.filter(img => !img.processed);
        break;
      default:
        break;
    }
    
    // Apply search
    if (searchQuery.trim()) {
      const query = searchQuery.toLowerCase();
      filtered = filtered.filter(img => 
        img.filename.toLowerCase().includes(query) ||
        img.tags?.some((tag: string) => tag.toLowerCase().includes(query))
      );
    }
    
    return filtered;
  }, [filter, searchQuery, storeItems]);
  
  // Handlers
  const handleImageOpen = useCallback((id: string) => {
    navigation.navigate('ImageDetail', { 
      id,
      projectId,
      imageUrl: storeItems.find(img => img.id === id)?.uri,
      imageTitle: storeItems.find(img => img.id === id)?.filename
    });
  }, [navigation, projectId, storeItems]);
  
  const handlePreset = useCallback(() => {
    if (!sel.selecting && sel.selectedIds.length === 0) {
      // Explicitly enter selection mode; UI tip handled by sheet or toast.
      sel.enter();
      toast('Tap photos to select. Long-press to add more.');
      return;
    }
    setPresetSheetVisible(true);
  }, [sel]);
  
  const handleShare = useCallback(() => {
    if (!sel.selecting && sel.selectedIds.length === 0) {
      sel.enter();
      toast('Tap photos to select. Long-press to add more.');
      return;
    }
    setShareSheetVisible(true);
  }, [sel]);
  
  const handleClear = useCallback(() => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    sel.clear();
  }, [sel]);
  
  const handleMore = useCallback(() => {
    setMoreMenuVisible(true);
  }, []);
  
  const handleAutomationSettings = useCallback(() => {
    setMoreMenuVisible(false);
    navigation.navigate('AutomationSettings', { projectId });
  }, [navigation, projectId]);
  
  const handleSelectAll = useCallback(() => {
    sel.set(filteredImages.map(img => img.id));
    setMoreMenuVisible(false);
  }, [sel, filteredImages]);
  
  const handleDeselectAll = useCallback(() => {
    sel.clear();
    setMoreMenuVisible(false);
  }, [sel]);
  
  const handleChangeView = useCallback(() => {
    toast('View change not implemented yet');
    setMoreMenuVisible(false);
  }, []);
  
  const handleSort = useCallback(() => {
    toast('Sort not implemented yet');
    setMoreMenuVisible(false);
  }, []);
  
  const handleUpload = useCallback(() => {
    toast('Upload not implemented yet');
  }, []);
  
  const handlePresetApply = useCallback((presetId: string) => {
    toast(`Applied preset ${presetId} to ${sel.selectedIds.length} images`);
    setPresetSheetVisible(false);
    sel.clear();
  }, [sel]);
  
  const handleShareAction = useCallback(() => {
    toast(`Shared ${sel.selectedIds.length} images`);
    setShareSheetVisible(false);
  }, [sel]);
  
  return (
    <SafeAreaView style={styles.safe}>
      {/* Header */}
      <View style={styles.header}>
        <TouchableOpacity 
          style={styles.backButton} 
          onPress={() => navigation.goBack()}
          accessibilityRole="button"
          accessibilityLabel="Go back"
        >
          <Ionicons name="arrow-back" size={24} color={colors.text} />
          <Text style={styles.backText}>Back</Text>
        </TouchableOpacity>
        
        <View style={styles.titleContainer}>
          <Text style={styles.title}>{projectName}</Text>
          <Text style={styles.subtitle}>
            {projectLoading ? "Loading..." : projectSubtitle}
          </Text>
        </View>
        
        <TouchableOpacity 
          style={styles.moreButton} 
          onPress={handleMore}
          accessibilityRole="button"
          accessibilityLabel="More options"
        >
          <Ionicons name="ellipsis-horizontal" size={24} color={colors.text} />
        </TouchableOpacity>
      </View>
      
      {/* Stats */}
      <View style={styles.statsContainer}>
        <View style={styles.statsChip}>
          <Ionicons name="checkmark-circle" size={16} color={colors.success} />
          <Text style={styles.statsText}>
            {projectData?.photoCount || 0} photos • {storeItems.filter(img => img.processed).length} processed
          </Text>
        </View>
      </View>
      
      {/* Tags */}
      {projectTags.length > 0 && (
        <View style={styles.tagsContainer}>
          {projectTags.map(tag => (
            <View key={tag} style={styles.tagChip}>
              <Text style={styles.tagText}>{tag}</Text>
            </View>
          ))}
        </View>
      )}
      
      {/* Filters */}
      <GalleryFilters
        filter={filter}
        onChangeFilter={setFilter}
        query={searchQuery}
        onChangeQuery={setSearchQuery}
        onUpload={handleUpload}
        gridWidth={targetCardWidth}
        onChangeGridWidth={setTargetCardWidth}
      />
      
      {/* Gallery Grid */}
      {photosLoading ? (
        <View style={styles.loadingContainer}>
          <Text style={styles.loadingText}>Loading photos...</Text>
        </View>
      ) : photosError ? (
        <View style={styles.errorContainer}>
          <Text style={styles.errorText}>Error loading photos: {photosError}</Text>
        </View>
      ) : storeItems.length === 0 ? (
        <View style={styles.emptyContainer}>
          <Text style={styles.emptyText}>No photos found for this project</Text>
        </View>
      ) : (
        <GalleryGrid
          items={filteredImages}
          selectedIds={sel.selectedIds}
          selecting={sel.selecting}                 // NEW
          onToggleSelect={sel.toggle}
          onSelectFirst={(id) => {                  // NEW: used by long-press
            if (!sel.selecting) sel.enter();
            sel.select(id);
          }}
          onOpenDetail={handleImageOpen}
          targetCardWidth={targetCardWidth}
        />
      )}
      
      {/* Sticky Action Bar */}
      <StickyActionBar
        selectedCount={sel.selectedIds.length}
        selecting={sel.selecting}
        onPreset={handlePreset}
        onShare={handleShare}
        onClear={() => sel.exit()} // exit selection mode completely
        onMore={handleMore}
        onDone={() => sel.exit()} // exit selection mode when Done is pressed
      />
      
      {/* Sheets and Modals */}
      <PresetPickerSheet
        visible={presetSheetVisible}
        onClose={() => setPresetSheetVisible(false)}
        onApply={handlePresetApply}
        selectedCount={sel.selectedIds.length}
      />
      
      <ShareSheet
        visible={shareSheetVisible}
        onClose={() => setShareSheetVisible(false)}
        onShare={handleShareAction}
        selectedCount={sel.selectedIds.length}
      />
      
      <MoreMenu
        visible={moreMenuVisible}
        onClose={() => setMoreMenuVisible(false)}
        onAutomationSettings={handleAutomationSettings}
        onSelectAll={handleSelectAll}
        onDeselectAll={handleDeselectAll}
        onChangeView={handleChangeView}
        onSort={handleSort}
      />
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  safe: { 
    flex: 1, 
    backgroundColor: colors.bg 
  },
  
  header: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: space.lg,
    paddingVertical: space.lg,
    backgroundColor: colors.surface,
    ...shadow.card,
  },
  
  backButton: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingVertical: space.sm,
    paddingHorizontal: space.sm,
  },
  
  backText: {
    marginLeft: space.xs,
    ...T.body,
    fontWeight: '600',
  },
  
  titleContainer: {
    flex: 1,
    alignItems: 'center',
  },
  
  title: {
    ...T.title,
    textAlign: 'center',
  },
  
  subtitle: {
    ...T.small,
    textAlign: 'center',
    marginTop: space.xs,
  },
  
  moreButton: {
    padding: space.sm,
  },
  
  statsContainer: {
    paddingHorizontal: space.lg,
    marginTop: space.xl,
    marginBottom: space.md,
  },
  
  statsChip: {
    alignSelf: 'flex-start',
    flexDirection: 'row',
    alignItems: 'center',
    gap: space.sm,
    backgroundColor: '#DDF3E5',
    borderRadius: radius.pill,
    paddingHorizontal: space.md,
    paddingVertical: space.sm,
  },
  
  statsText: {
    color: colors.brand,
    fontWeight: '700',
    fontSize: 12,
  },
  
  tagsContainer: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: space.sm,
    paddingHorizontal: space.lg,
    marginTop: space.lg,
    marginBottom: space.lg,
  },
  
  tagChip: {
    backgroundColor: '#EEE8DF',
    paddingHorizontal: space.md,
    paddingVertical: space.sm,
    borderRadius: radius.pill,
  },
  
  tagText: {
    color: colors.text,
    fontSize: 12,
    fontWeight: '700',
  },
  
  loadingContainer: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    paddingVertical: space.xl,
  },
  
  loadingText: {
    ...T.body,
    color: colors.subtext,
  },
  
  errorContainer: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    paddingVertical: space.xl,
  },
  
  errorText: {
    ...T.body,
    color: colors.danger,
    textAlign: 'center',
  },
  
  emptyContainer: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    paddingVertical: space.xl,
  },
  
  emptyText: {
    ...T.body,
    color: colors.subtext,
    textAlign: 'center',
  },
});