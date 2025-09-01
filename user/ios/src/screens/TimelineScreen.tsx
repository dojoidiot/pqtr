import React, { useState, useRef } from 'react';
import {
  SafeAreaView,
  Text,
  View,
  TouchableOpacity,
  StyleSheet,
  ScrollView,
  Dimensions,
  Animated,
  Image,
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import * as Haptics from 'expo-haptics';
import { theme } from '../constants/theme';

const { width: screenWidth } = Dimensions.get('window');

interface TimelineItem {
  id: string;
  imageUrl: string;
  thumbnail: string;
  title: string;
  status: 'ingested' | 'edited' | 'exported' | 'published';
  timestamp: string;
  camera?: string;
  location?: string;
  tags: string[];
  presetApplied?: string;
}

const mockTimelineData: TimelineItem[] = [
  {
    id: '1',
    imageUrl: 'https://images.unsplash.com/photo-1506905925346-21bda4d32df4?w=400',
    thumbnail: 'https://images.unsplash.com/photo-1506905925346-21bda4d32df4?w=100',
    title: 'Mountain Landscape',
    status: 'published',
    timestamp: '2024-01-15T10:30:00Z',
    camera: 'Canon EOS R5',
    location: 'Rocky Mountains, CO',
    tags: ['landscape', 'mountains', 'nature'],
    presetApplied: 'Vivid Landscape',
  },
  {
    id: '2',
    imageUrl: 'https://images.unsplash.com/photo-1441974231531-c6227db76b6e?w=400',
    thumbnail: 'https://images.unsplash.com/photo-1441974231531-c6227db76b6e?w=100',
    title: 'Forest Path',
    status: 'exported',
    timestamp: '2024-01-15T09:15:00Z',
    camera: 'Sony A7R IV',
    location: 'Redwood Forest, CA',
    tags: ['forest', 'path', 'green'],
    presetApplied: 'Moody Forest',
  },
  {
    id: '3',
    imageUrl: 'https://images.unsplash.com/photo-1506905925346-21bda4d32df4?w=400',
    thumbnail: 'https://images.unsplash.com/photo-1506905925346-21bda4d32df4?w=100',
    title: 'City Skyline',
    status: 'edited',
    timestamp: '2024-01-15T08:45:00Z',
    camera: 'Nikon Z9',
    location: 'New York, NY',
    tags: ['city', 'skyline', 'urban'],
    presetApplied: 'Urban Contrast',
  },
  {
    id: '4',
    imageUrl: 'https://images.unsplash.com/photo-1441974231531-c6227db76b6e?w=400',
    thumbnail: 'https://images.unsplash.com/photo-1441974231531-c6227db76b6e?w=100',
    title: 'Ocean Waves',
    status: 'ingested',
    timestamp: '2024-01-15T07:20:00Z',
    camera: 'Fujifilm GFX 100S',
    location: 'Pacific Coast, CA',
    tags: ['ocean', 'waves', 'blue'],
  },
];

const TimelineScreen: React.FC = () => {
  const [selectedFilter, setSelectedFilter] = useState<'all' | 'ingested' | 'edited' | 'exported' | 'published'>('all');
  const [selectedItem, setSelectedItem] = useState<TimelineItem | null>(null);
  const scrollViewRef = useRef<ScrollView>(null);
  const fadeAnim = useRef(new Animated.Value(0)).current;

  React.useEffect(() => {
    Animated.timing(fadeAnim, {
      toValue: 1,
      duration: 600,
      useNativeDriver: true,
    }).start();
  }, []);

  const filters = [
    { key: 'all', label: 'All', icon: 'grid-outline' },
    { key: 'ingested', label: 'Ingested', icon: 'cloud-download-outline' },
    { key: 'edited', label: 'Edited', icon: 'brush-outline' },
    { key: 'exported', label: 'Exported', icon: 'download-outline' },
    { key: 'published', label: 'Published', icon: 'globe-outline' },
  ];

  const filteredData = selectedFilter === 'all' 
    ? mockTimelineData 
    : mockTimelineData.filter(item => item.status === selectedFilter);

  const handleFilterPress = (filterKey: typeof selectedFilter) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    setSelectedFilter(filterKey);
  };

  const handleItemPress = (item: TimelineItem) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Medium);
    setSelectedItem(item);
  };

  const handleClosePreview = () => {
    setSelectedItem(null);
  };

  const getStatusColor = (status: TimelineItem['status']) => {
    switch (status) {
      case 'ingested': return theme.colors.system.blue;
      case 'edited': return theme.colors.system.orange;
      case 'exported': return theme.colors.system.green;
      case 'published': return theme.colors.system.purple;
      default: return theme.colors.gray;
    }
  };

  const getStatusIcon = (status: TimelineItem['status']) => {
    switch (status) {
      case 'ingested': return 'cloud-download-outline';
      case 'edited': return 'brush-outline';
      case 'exported': return 'download-outline';
      case 'published': return 'globe-outline';
      default: return 'ellipse-outline';
    }
  };

  const formatTimestamp = (timestamp: string) => {
    const date = new Date(timestamp);
    const now = new Date();
    const diffInHours = Math.floor((now.getTime() - date.getTime()) / (1000 * 60 * 60));
    
    if (diffInHours < 1) return 'Just now';
    if (diffInHours < 24) return `${diffInHours}h ago`;
    return date.toLocaleDateString();
  };

  return (
    <SafeAreaView style={styles.container}>
      {/* Header */}
      <View style={styles.header}>
        <Text style={styles.headerTitle}>Timeline</Text>
        <TouchableOpacity style={styles.headerButton}>
          <Ionicons name="options-outline" size={24} color={theme.colors.text.secondary} />
        </TouchableOpacity>
      </View>

      {/* Filter Tabs */}
      <ScrollView 
        horizontal 
        showsHorizontalScrollIndicator={false}
        contentContainerStyle={styles.filterContainer}
        ref={scrollViewRef}
      >
        {filters.map((filter) => (
          <TouchableOpacity
            key={filter.key}
            style={[
              styles.filterTab,
              selectedFilter === filter.key && styles.filterTabActive
            ]}
            onPress={() => handleFilterPress(filter.key as typeof selectedFilter)}
          >
            <Ionicons 
              name={filter.icon as keyof typeof Ionicons.glyphMap} 
              size={16} 
              color={selectedFilter === filter.key ? theme.colors.surface : theme.colors.text.secondary} 
            />
            <Text style={[
              styles.filterText,
              selectedFilter === filter.key && styles.filterTextActive
            ]}>
              {filter.label}
            </Text>
          </TouchableOpacity>
        ))}
      </ScrollView>

      {/* Timeline Content */}
      <Animated.View style={[styles.timelineContainer, { opacity: fadeAnim }]}>
        <ScrollView 
          style={styles.timelineScroll}
          showsVerticalScrollIndicator={false}
          contentContainerStyle={styles.timelineContent}
        >
          {filteredData.map((item, index) => (
            <View key={item.id} style={styles.timelineItem}>
              {/* Timeline Line */}
              <View style={styles.timelineLine}>
                <View style={[styles.timelineDot, { backgroundColor: getStatusColor(item.status) }]} />
                {index < filteredData.length - 1 && <View style={styles.timelineConnector} />}
              </View>

              {/* Content Card */}
              <TouchableOpacity 
                style={styles.contentCard}
                onPress={() => handleItemPress(item)}
                activeOpacity={0.8}
              >
                {/* Image Thumbnail */}
                <View style={styles.imageContainer}>
                  <Image source={{ uri: item.thumbnail }} style={styles.thumbnail} />
                  <View style={[styles.statusBadge, { backgroundColor: getStatusColor(item.status) }]}>
                    <Ionicons 
                      name={getStatusIcon(item.status) as keyof typeof Ionicons.glyphMap} 
                      size={12} 
                      color={theme.colors.surface} 
                    />
                  </View>
                </View>

                {/* Content Details */}
                <View style={styles.contentDetails}>
                  <Text style={styles.itemTitle} numberOfLines={1}>
                    {item.title}
                  </Text>
                  
                  <View style={styles.metadataRow}>
                    <Ionicons name="time-outline" size={14} color={theme.colors.text.tertiary} />
                    <Text style={styles.metadataText}>
                      {formatTimestamp(item.timestamp)}
                    </Text>
                  </View>

                  {item.camera && (
                    <View style={styles.metadataRow}>
                      <Ionicons name="camera-outline" size={14} color={theme.colors.text.tertiary} />
                      <Text style={styles.metadataText} numberOfLines={1}>
                        {item.camera}
                      </Text>
                    </View>
                  )}

                  {item.location && (
                    <View style={styles.metadataRow}>
                      <Ionicons name="location-outline" size={14} color={theme.colors.text.tertiary} />
                      <Text style={styles.metadataText} numberOfLines={1}>
                        {item.location}
                      </Text>
                    </View>
                  )}

                  {/* Tags */}
                  {item.tags.length > 0 && (
                    <View style={styles.tagsContainer}>
                      {item.tags.slice(0, 3).map((tag, tagIndex) => (
                        <View key={tagIndex} style={styles.tagChip}>
                          <Text style={styles.tagText}>#{tag}</Text>
                        </View>
                      ))}
                      {item.tags.length > 3 && (
                        <Text style={styles.moreTagsText}>+{item.tags.length - 3}</Text>
                      )}
                    </View>
                  )}

                  {/* Preset Applied */}
                  {item.presetApplied && (
                    <View style={styles.presetBadge}>
                      <Ionicons name="brush-outline" size={12} color={theme.colors.green} />
                      <Text style={styles.presetText}>{item.presetApplied}</Text>
                    </View>
                  )}
                </View>

                {/* Action Button */}
                <TouchableOpacity style={styles.actionButton}>
                  <Ionicons name="chevron-forward" size={20} color={theme.colors.text.tertiary} />
                </TouchableOpacity>
              </TouchableOpacity>
            </View>
          ))}
        </ScrollView>
      </Animated.View>

      {/* Image Preview Modal */}
      {selectedItem && (
        <View style={styles.previewOverlay}>
          <View style={styles.previewContainer}>
            <View style={styles.previewHeader}>
              <Text style={styles.previewTitle}>{selectedItem.title}</Text>
              <TouchableOpacity onPress={handleClosePreview} style={styles.closePreviewButton}>
                <Ionicons name="close" size={24} color={theme.colors.text.secondary} />
              </TouchableOpacity>
            </View>
            
            <Image source={{ uri: selectedItem.imageUrl }} style={styles.previewImage} />
            
            <View style={styles.previewDetails}>
              <View style={styles.previewMetadata}>
                <Text style={styles.previewMetadataText}>
                  <Text style={styles.previewLabel}>Status: </Text>
                  {selectedItem.status.charAt(0).toUpperCase() + selectedItem.status.slice(1)}
                </Text>
                <Text style={styles.previewMetadataText}>
                  <Text style={styles.previewLabel}>Time: </Text>
                  {formatTimestamp(selectedItem.timestamp)}
                </Text>
                {selectedItem.camera && (
                  <Text style={styles.previewMetadataText}>
                    <Text style={styles.previewLabel}>Camera: </Text>
                    {selectedItem.camera}
                  </Text>
                )}
                {selectedItem.location && (
                  <Text style={styles.previewMetadataText}>
                    <Text style={styles.previewLabel}>Location: </Text>
                    {selectedItem.location}
                  </Text>
                )}
              </View>
              
              <View style={styles.previewActions}>
                <TouchableOpacity style={styles.previewActionButton}>
                  <Ionicons name="brush-outline" size={20} color={theme.colors.text.primary} />
                  <Text style={styles.previewActionText}>Edit</Text>
                </TouchableOpacity>
                <TouchableOpacity style={styles.previewActionButton}>
                  <Ionicons name="download-outline" size={20} color={theme.colors.text.primary} />
                  <Text style={styles.previewActionText}>Export</Text>
                </TouchableOpacity>
                <TouchableOpacity style={styles.previewActionButton}>
                  <Ionicons name="share-outline" size={20} color={theme.colors.text.primary} />
                  <Text style={styles.previewActionText}>Share</Text>
                </TouchableOpacity>
              </View>
            </View>
          </View>
        </View>
      )}
    </SafeAreaView>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: theme.colors.background,
  },
  header: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingHorizontal: theme.spacing.l,
    paddingVertical: theme.spacing.m,
  },
  headerTitle: {
    fontSize: theme.fontSizes.largeTitle,
    fontWeight: '700',
    color: theme.colors.text.primary,
    letterSpacing: -0.5,
  },
  headerButton: {
    padding: theme.spacing.s,
  },
  filterContainer: {
    paddingHorizontal: theme.spacing.l,
    paddingBottom: theme.spacing.m,
  },
  filterTab: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: theme.spacing.m,
    paddingVertical: theme.spacing.s,
    marginRight: theme.spacing.s,
    backgroundColor: theme.colors.lightGray,
    borderRadius: theme.borderRadius.m,
    gap: theme.spacing.xs,
  },
  filterTabActive: {
    backgroundColor: theme.colors.green,
  },
  filterText: {
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.text.secondary,
  },
  filterTextActive: {
    color: theme.colors.surface,
  },
  timelineContainer: {
    flex: 1,
  },
  timelineScroll: {
    flex: 1,
  },
  timelineContent: {
    paddingHorizontal: theme.spacing.l,
    paddingBottom: theme.spacing.xl,
  },
  timelineItem: {
    flexDirection: 'row',
    marginBottom: theme.spacing.l,
  },
  timelineLine: {
    alignItems: 'center',
    marginRight: theme.spacing.m,
  },
  timelineDot: {
    width: 12,
    height: 12,
    borderRadius: 6,
    ...theme.shadows.subtle,
  },
  timelineConnector: {
    width: 2,
    height: 60,
    backgroundColor: theme.colors.border,
    marginTop: theme.spacing.xs,
  },
  contentCard: {
    flex: 1,
    flexDirection: 'row',
    backgroundColor: theme.colors.surface,
    borderRadius: theme.borderRadius.l,
    padding: theme.spacing.m,
    ...theme.shadows.subtle,
    borderWidth: 1,
    borderColor: theme.colors.border,
  },
  imageContainer: {
    position: 'relative',
    marginRight: theme.spacing.m,
  },
  thumbnail: {
    width: 80,
    height: 80,
    borderRadius: theme.borderRadius.m,
  },
  statusBadge: {
    position: 'absolute',
    top: -4,
    right: -4,
    width: 20,
    height: 20,
    borderRadius: 10,
    justifyContent: 'center',
    alignItems: 'center',
    ...theme.shadows.subtle,
  },
  contentDetails: {
    flex: 1,
    justifyContent: 'space-between',
  },
  itemTitle: {
    fontSize: theme.fontSizes.headline,
    fontWeight: '700',
    color: theme.colors.text.primary,
    marginBottom: theme.spacing.xs,
  },
  metadataRow: {
    flexDirection: 'row',
    alignItems: 'center',
    marginBottom: theme.spacing.xs,
    gap: theme.spacing.xs,
  },
  metadataText: {
    fontSize: theme.fontSizes.caption,
    color: theme.colors.text.tertiary,
    fontWeight: '500',
  },
  tagsContainer: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: theme.spacing.xs,
    marginTop: theme.spacing.xs,
  },
  tagChip: {
    backgroundColor: theme.colors.lightGray,
    paddingHorizontal: theme.spacing.xs,
    paddingVertical: 2,
    borderRadius: theme.borderRadius.s,
  },
  tagText: {
    fontSize: theme.fontSizes.caption,
    color: theme.colors.text.secondary,
    fontWeight: '500',
  },
  moreTagsText: {
    fontSize: theme.fontSizes.caption,
    color: theme.colors.text.tertiary,
    fontWeight: '500',
    alignSelf: 'center',
  },
  presetBadge: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: theme.colors.green + '10',
    paddingHorizontal: theme.spacing.xs,
    paddingVertical: 2,
    borderRadius: theme.borderRadius.s,
    alignSelf: 'flex-start',
    marginTop: theme.spacing.xs,
    gap: theme.spacing.xs,
  },
  presetText: {
    fontSize: theme.fontSizes.caption,
    color: theme.colors.green,
    fontWeight: '600',
  },
  actionButton: {
    justifyContent: 'center',
    paddingLeft: theme.spacing.s,
  },
  previewOverlay: {
    position: 'absolute',
    top: 0,
    left: 0,
    right: 0,
    bottom: 0,
    backgroundColor: 'rgba(0, 0, 0, 0.8)',
    justifyContent: 'center',
    alignItems: 'center',
    zIndex: 1000,
  },
  previewContainer: {
    backgroundColor: theme.colors.surface,
    borderRadius: theme.borderRadius.xl,
    width: '90%',
    maxHeight: '80%',
    ...theme.shadows.large,
  },
  previewHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    padding: theme.spacing.l,
    borderBottomWidth: 1,
    borderBottomColor: theme.colors.border,
  },
  previewTitle: {
    fontSize: theme.fontSizes.title,
    fontWeight: '700',
    color: theme.colors.text.primary,
  },
  closePreviewButton: {
    padding: theme.spacing.s,
  },
  previewImage: {
    width: '100%',
    height: 300,
    resizeMode: 'cover',
  },
  previewDetails: {
    padding: theme.spacing.l,
  },
  previewMetadata: {
    marginBottom: theme.spacing.l,
  },
  previewMetadataText: {
    fontSize: theme.fontSizes.body,
    color: theme.colors.text.primary,
    marginBottom: theme.spacing.xs,
  },
  previewLabel: {
    fontWeight: '600',
    color: theme.colors.text.secondary,
  },
  previewActions: {
    flexDirection: 'row',
    gap: theme.spacing.m,
  },
  previewActionButton: {
    flex: 1,
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'center',
    paddingVertical: theme.spacing.m,
    paddingHorizontal: theme.spacing.m,
    backgroundColor: theme.colors.lightGray,
    borderRadius: theme.borderRadius.m,
    gap: theme.spacing.xs,
  },
  previewActionText: {
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.text.primary,
  },
});

export default TimelineScreen;
