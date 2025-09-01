import React, { useState, useEffect, useMemo } from 'react';
import {
  View,
  Text,
  StyleSheet,
  SafeAreaView,
  Animated,
  FlatList,
  TouchableOpacity,
  Alert,
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import { useNavigation } from '@react-navigation/native';
import * as Haptics from 'expo-haptics';
import { theme } from '../constants/theme';
import ProjectCard from '../components/ProjectCard';
import ProjectListItem from '../components/ProjectListItem';
import ViewModeAndSort from '../components/ViewModeAndSort';
import { useCurrentUser, useProjects } from '../hooks/useDatabase';
import UploadProgressPill from '../components/UploadProgressPill';

interface HomeScreenProps {
  navigation: any;
}

const HomeScreen: React.FC<HomeScreenProps> = ({ navigation }) => {
  const [isCardView, setIsCardView] = useState(true);
  const [searchQuery, setSearchQuery] = useState('');
  const [sortBy, setSortBy] = useState<'recent' | 'alpha' | 'imgs_desc' | 'imgs_asc' | 'oldest'>('recent');
  const fadeAnim = useState(new Animated.Value(0))[0];
  
  const { user: currentUser } = useCurrentUser();
  const { projects, loading, error } = useProjects(currentUser?.id || '');

  // Debug logging
  useEffect(() => {
    console.log('Current User:', currentUser);
    console.log('Projects:', projects);
    console.log('Loading:', loading);
    console.log('Error:', error);
  }, [currentUser, projects, loading, error]);

  useEffect(() => {
    Animated.timing(fadeAnim, {
      toValue: 1,
      duration: 500,
      useNativeDriver: true,
    }).start();
  }, []);

  const handleCreateProject = () => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Medium);
    navigation.navigate('CreateProject');
  };

  const handleProjectPress = (project: any) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Medium);
    console.log('HomeScreen - Navigating to project:', {
      projectId: project.id,
      projectTitle: project.name,
      fullProject: project
    });
    navigation.navigate('Project', {
      projectTitle: project.name,
      projectId: project.id,
    });
  };

  const handlePresets = () => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    navigation.navigate('PresetManager');
  };

  const handleShareProject = (project: any) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Medium);
    Alert.alert('Share Project', `Share project "${project.name}"`);
  };

  const handleProjectMenu = (project: any) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    Alert.alert(
      'Project Options',
      `Options for "${project.name}"`,
      [
        { text: 'Share', onPress: () => handleShareProject(project) },
        { text: 'Rename', onPress: () => console.log('Rename project:', project.name) },
        { text: 'Delete', onPress: () => console.log('Delete project:', project.name), style: 'destructive' },
        { text: 'Cancel', style: 'cancel' }
      ]
    );
  };

  const handleSortChange = (newSort: 'recent' | 'alpha' | 'imgs_desc' | 'imgs_asc' | 'oldest') => {
    setSortBy(newSort);
  };

  const renderProjectCard = ({ item }: { item: any }) => {
    console.log('Rendering project card for:', {
      id: item.id,
      name: item.name,
      photoCount: item.photoCount,
      coverImage: item.coverImage,
      updatedAt: item.updatedAt,
      userId: item.userId,
      uploadProgress: item.uploadProgress,
      uploadStatus: item.uploadStatus,
    });

    // Map upload status to sync state
    const getSyncState = (uploadStatus?: string, uploadProgress?: number): 'synced' | 'uploading' | 'queued' | 'paused' | 'error' => {
      if (uploadProgress === 1 || uploadStatus === 'synced') return 'synced';
      if (uploadStatus === 'error') return 'error';
      if (uploadStatus === 'paused') return 'paused';
      if (uploadStatus === 'queued') return 'queued';
      if (uploadStatus === 'uploading') return 'uploading';
      return 'synced'; // default to synced if no status
    };

    // Format date for display
    const formatDate = (timestamp: string) => {
      const date = new Date(timestamp);
      return date.toLocaleDateString('en-US', {
        month: 'short',
        day: 'numeric',
        year: 'numeric'
      });
    };

    return (
      <View style={styles.projectCardWrapper}>
        <ProjectCard
          coverUri={item.coverImage || 'https://images.unsplash.com/photo-1506905925346-21bda4d32df4?w=1200'}
          title={item.name}
          imagesCount={item.photoCount || 0}
          lastUpdated={formatDate(item.updatedAt)}
          syncState={getSyncState(item.uploadStatus, item.uploadProgress)}
          progress={item.uploadProgress || 0}
          onPress={() => handleProjectPress(item)}
          onMenuPress={() => handleProjectMenu(item)}
        />
      </View>
    );
  };

  const renderProjectListItem = ({ item }: { item: any }) => {
    // Map upload status to sync state
    const getSyncState = (uploadStatus?: string, uploadProgress?: number): 'synced' | 'uploading' | 'queued' | 'paused' | 'error' => {
      if (uploadProgress === 1 || uploadStatus === 'synced') return 'synced';
      if (uploadStatus === 'error') return 'error';
      if (uploadStatus === 'paused') return 'paused';
      if (uploadStatus === 'queued') return 'queued';
      if (uploadStatus === 'uploading') return 'uploading';
      return 'synced'; // default to synced if no status
    };

    // Format date for display
    const formatDate = (timestamp: string) => {
      const date = new Date(timestamp);
      return date.toLocaleDateString('en-US', {
        month: 'short',
        day: 'numeric',
        year: 'numeric'
      });
    };

    return (
      <ProjectListItem
        title={item.name}
        imagesCount={item.photoCount || 0}
        lastSyncedLabel={getTimeAgo(item.updatedAt)}
        createdLabel={formatDate(item.updatedAt)}
        syncState={getSyncState(item.uploadStatus, item.uploadProgress)}
        progress={item.uploadProgress || 0}
        onPress={() => handleProjectPress(item)}
        onShare={() => handleShareProject(item)}
        onMore={() => {
          // TODO: Implement action sheet
          console.log('More options for project:', item.name);
        }}
      />
    );
  };

  const filteredProjects = useMemo(() => {
    let filtered = projects.filter(project =>
      project.name.toLowerCase().includes(searchQuery.toLowerCase())
    );

    // Apply sorting
    switch (sortBy) {
      case 'alpha':
        filtered.sort((a, b) => a.name.localeCompare(b.name));
        break;
      case 'imgs_desc':
        filtered.sort((a, b) => (b.photoCount || 0) - (a.photoCount || 0));
        break;
      case 'imgs_asc':
        filtered.sort((a, b) => (a.photoCount || 0) - (b.photoCount || 0));
        break;
      case 'oldest':
        filtered.sort((a, b) => new Date(a.updatedAt).getTime() - new Date(b.updatedAt).getTime());
        break;
      case 'recent':
      default:
        filtered.sort((a, b) => new Date(b.updatedAt).getTime() - new Date(a.updatedAt).getTime());
        break;
    }

    return filtered;
  }, [projects, searchQuery, sortBy]);

  if (loading) {
    return (
      <SafeAreaView style={styles.container}>
        <View style={styles.loadingContainer}>
          <Text style={styles.loadingText}>Loading projects...</Text>
        </View>
      </SafeAreaView>
    );
  }

  if (error) {
    return (
      <SafeAreaView style={styles.container}>
        <View style={styles.errorContainer}>
          <Text style={styles.errorText}>Error loading projects: {error}</Text>
        </View>
      </SafeAreaView>
    );
  }

  return (
    <SafeAreaView style={styles.container}>
      {/* Header with Logo */}
      <View style={styles.header}>
        <Text style={styles.headerTitle}>PQTR</Text>
      </View>

      {/* Search Bar */}
      <View style={styles.searchContainer}>
        <View style={styles.searchBar}>
          <Ionicons name="search" size={20} color={theme.colors.text.secondary} />
          <Text style={styles.searchInput}>
            {searchQuery || 'Search for anything'}
          </Text>
        </View>
      </View>

      {/* Action Buttons - Add & Presets */}
      <View style={styles.actionButtons}>
        <TouchableOpacity style={styles.actionButton} onPress={handleCreateProject}>
          <Ionicons name="add-circle-outline" size={20} color={theme.colors.accent} />
          <Text style={styles.actionButtonText}>New Project</Text>
        </TouchableOpacity>
        <TouchableOpacity style={styles.actionButton} onPress={handlePresets}>
          <Text style={styles.presetEmoji}>🎨</Text>
          <Text style={styles.actionButtonText}>Presets</Text>
        </TouchableOpacity>
      </View>

      {/* View Toggle and Sort Bar */}
      <View style={styles.controlsBar}>
        {/* View Mode Toggle */}
        <View style={styles.segmentedControl}>
          <TouchableOpacity 
            style={[styles.segmentButton, isCardView && styles.segmentButtonActive]}
            onPress={() => setIsCardView(true)}
            activeOpacity={0.8}
          >
            <Ionicons 
              name="grid-outline" 
              size={18} 
              color={isCardView ? theme.colors.surface : theme.colors.text.secondary} 
            />
          </TouchableOpacity>
          <TouchableOpacity 
            style={[styles.segmentButton, !isCardView && styles.segmentButtonActive]}
            onPress={() => setIsCardView(false)}
            activeOpacity={0.8}
          >
            <Ionicons 
              name="list-outline" 
              size={18} 
              color={!isCardView ? theme.colors.surface : theme.colors.text.secondary} 
            />
          </TouchableOpacity>
        </View>
        
        {/* Sort Control */}
        <View style={styles.sortSection}>
          <ViewModeAndSort
            sort={sortBy}
            onSortChange={handleSortChange}
          />
        </View>
      </View>

      {/* Projects List */}
      <Animated.View style={[styles.projectsSection, { opacity: fadeAnim }]}>
        <FlatList
          data={filteredProjects}
          renderItem={isCardView ? renderProjectCard : renderProjectListItem}
          keyExtractor={(item) => item.id}
          showsVerticalScrollIndicator={false}
          contentContainerStyle={[
            styles.projectsList,
            !isCardView && { paddingHorizontal: theme.spacing.m }
          ]}
          ItemSeparatorComponent={() => !isCardView ? <View style={{ height: 12 }} /> : null}
        />
      </Animated.View>


    </SafeAreaView>
  );
};

const getTimeAgo = (timestamp: string) => {
  const now = new Date();
  const date = new Date(timestamp);
  const diffInSeconds = Math.floor((now.getTime() - date.getTime()) / 1000);

  if (diffInSeconds < 60) {
    return `${diffInSeconds}s ago`;
  } else if (diffInSeconds < 3600) {
    return `${Math.floor(diffInSeconds / 60)}m ago`;
  } else if (diffInSeconds < 86400) {
    return `${Math.floor(diffInSeconds / 3600)}h ago`;
  } else {
    return `${Math.floor(diffInSeconds / 86400)}d ago`;
  }
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: theme.colors.background,
  },
  header: {
    paddingTop: theme.spacing.l,
    paddingBottom: theme.spacing.m,
    paddingHorizontal: theme.spacing.l,
  },
  headerTitle: {
    fontSize: theme.fontSizes.largeTitle,
    fontWeight: '700',
    color: theme.colors.text.primary,
    letterSpacing: -0.5,
  },
  searchContainer: {
    paddingHorizontal: theme.spacing.l,
    marginBottom: theme.spacing.m,
  },
  searchBar: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: theme.colors.surface,
    borderRadius: theme.borderRadius.l,
    paddingHorizontal: theme.spacing.m,
    paddingVertical: theme.spacing.m,
    borderWidth: 1,
    borderColor: 'rgba(0,0,0,0.05)',
    ...theme.shadows.subtle,
  },
  searchInput: {
    flex: 1,
    marginLeft: theme.spacing.m,
    fontSize: theme.fontSizes.body,
    color: theme.colors.text.primary,
    fontWeight: '400',
  },
  actionButtons: {
    flexDirection: 'row',
    paddingHorizontal: theme.spacing.l,
    marginBottom: theme.spacing.l,
    gap: theme.spacing.s,
  },
  actionButton: {
    flex: 1,
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'center',
    backgroundColor: theme.colors.surface,
    paddingVertical: theme.spacing.m,
    paddingHorizontal: theme.spacing.l,
    borderRadius: theme.borderRadius.m,
    borderWidth: 1,
    borderColor: theme.colors.border,
    gap: theme.spacing.s,
    ...theme.shadows.subtle,
  },
  actionButtonText: {
    color: theme.colors.text.primary,
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
  },
  presetEmoji: {
    fontSize: 20,
  },
  controlsBar: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingHorizontal: theme.spacing.l,
    marginBottom: theme.spacing.l,
    paddingVertical: theme.spacing.m,
    backgroundColor: theme.colors.surface,
    borderRadius: theme.borderRadius.l,
    marginHorizontal: theme.spacing.l,
    borderWidth: 1,
    borderColor: theme.colors.border,
    gap: theme.spacing.l,
  },
  storageSection: {
    flex: 1,
    marginRight: theme.spacing.l,
  },
  storageLabel: {
    fontSize: theme.fontSizes.caption,
    color: theme.colors.text.secondary,
    fontWeight: '500',
    marginBottom: theme.spacing.xs,
  },
  progressBarContainer: {
    height: 4,
    backgroundColor: theme.colors.lightGray,
    borderRadius: 2,
    overflow: 'hidden',
  },
  progressBar: {
    height: '100%',
    backgroundColor: theme.colors.green,
    borderRadius: 2,
  },

  projectsSection: {
    flex: 1,
    paddingHorizontal: theme.spacing.m,
  },
  projectsList: {
    paddingBottom: theme.spacing.xl,
  },
  listItem: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    paddingVertical: theme.spacing.m,
    paddingHorizontal: theme.spacing.m,
    backgroundColor: theme.colors.surface,
    borderRadius: theme.borderRadius.l,
    marginBottom: theme.spacing.s,
    borderWidth: 1,
    borderColor: theme.colors.border,
  },
  listItemLeft: {
    flexDirection: 'row',
    alignItems: 'center',
    flex: 1,
  },
  folderIcon: {
    marginRight: theme.spacing.m,
  },
  listItemText: {
    flex: 1,
  },
  listItemTitle: {
    fontSize: theme.fontSizes.headline,
    fontWeight: '700',
    color: theme.colors.text.primary,
    marginBottom: 4,
    letterSpacing: -0.2,
  },
  listItemSubtitle: {
    fontSize: theme.fontSizes.body,
    fontWeight: '500',
    color: theme.colors.text.secondary,
    marginBottom: 2,
    letterSpacing: 0.1,
  },
  listItemSync: {
    fontSize: theme.fontSizes.caption,
    fontWeight: '600',
    color: theme.colors.text.tertiary,
    marginTop: 2,
    letterSpacing: 0.2,
  },
  listItemActions: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: theme.spacing.s,
  },
  listShareButton: {
    padding: theme.spacing.s,
    borderRadius: theme.borderRadius.s,
    backgroundColor: theme.colors.lightGray,
  },
  listItemMenu: {
    padding: theme.spacing.s,
  },
  listItemProgress: {
    marginRight: theme.spacing.s,
  },
  loadingContainer: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    backgroundColor: theme.colors.background,
  },
  loadingText: {
    fontSize: theme.fontSizes.body,
    color: theme.colors.text.primary,
  },
  errorContainer: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    backgroundColor: theme.colors.background,
  },
  errorText: {
    fontSize: theme.fontSizes.body,
    color: theme.colors.text.primary,
  },
  segmentedControl: {
    flexDirection: 'row',
    backgroundColor: theme.colors.lightGray,
    borderRadius: theme.borderRadius.s,
    borderWidth: 1,
    borderColor: theme.colors.border,
    padding: 2,
  },
  sortSection: {
    marginLeft: 'auto',
  },
  segmentButton: {
    paddingVertical: theme.spacing.xs,
    paddingHorizontal: theme.spacing.m,
    borderRadius: theme.borderRadius.s,
    alignItems: 'center',
    justifyContent: 'center',
    minWidth: 44,
  },
  segmentButtonActive: {
    backgroundColor: theme.colors.accent,
  },
  projectCardWrapper: {
    width: '100%',
    marginBottom: theme.spacing.m,
    paddingHorizontal: theme.spacing.s,
  },
});

export default HomeScreen; 