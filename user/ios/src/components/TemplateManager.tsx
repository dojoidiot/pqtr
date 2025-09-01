import React, { useState, useEffect, useRef } from 'react';
import {
  View,
  Text,
  TouchableOpacity,
  StyleSheet,
  ScrollView,
  TextInput,
  Switch,
  Alert,
  Animated,
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import * as Haptics from 'expo-haptics';
import { theme } from '../constants/theme';

interface ProjectTemplate {
  id: string;
  name: string;
  description: string;
  thumbnail: string;
  category: 'landscape' | 'portrait' | 'street' | 'wedding' | 'commercial' | 'custom';
  tags: string[];
  presets: string[];
  exportSettings: {
    format: 'JPEG' | 'PNG' | 'TIFF';
    resolution: 'Original' | '4K' | '2K' | '1080p';
    quality: number;
    includeMetadata: boolean;
  };
  metadata: {
    camera?: string;
    location?: string;
    style?: string;
  };
  usageCount: number;
  isFavorite: boolean;
  createdAt: string;
  createdBy: string;
}

interface TemplateManagerProps {
  isVisible: boolean;
  onClose: () => void;
  onUseTemplate: (template: ProjectTemplate) => void;
  onSaveTemplate: (template: Partial<ProjectTemplate>) => void;
}

const TemplateManager: React.FC<TemplateManagerProps> = ({
  isVisible,
  onClose,
  onUseTemplate,
  onSaveTemplate,
}) => {
  const [templates, setTemplates] = useState<ProjectTemplate[]>([]);
  const [selectedCategory, setSelectedCategory] = useState<'all' | ProjectTemplate['category']>('all');
  const [searchQuery, setSearchQuery] = useState('');
  const [showCreateForm, setShowCreateForm] = useState(false);
  const [selectedTemplate, setSelectedTemplate] = useState<ProjectTemplate | null>(null);
  const [showDetails, setShowDetails] = useState(false);
  
  const slideAnim = useRef(new Animated.Value(300)).current;
  const fadeAnim = useRef(new Animated.Value(0)).current;

  useEffect(() => {
    if (isVisible) {
      Animated.parallel([
        Animated.spring(slideAnim, {
          toValue: 0,
          useNativeDriver: true,
          tension: 100,
          friction: 8,
        }),
        Animated.timing(fadeAnim, {
          toValue: 1,
          duration: 300,
          useNativeDriver: true,
        }),
      ]).start();
      
      loadTemplates();
    } else {
      Animated.parallel([
        Animated.spring(slideAnim, {
          toValue: 300,
          useNativeDriver: true,
          tension: 100,
          friction: 8,
        }),
        Animated.timing(fadeAnim, {
          toValue: 0,
          duration: 200,
          useNativeDriver: true,
        }),
      ]).start();
    }
  }, [isVisible]);

  const loadTemplates = () => {
    const mockTemplates: ProjectTemplate[] = [
      {
        id: '1',
        name: 'Mountain Landscape',
        description: 'Perfect for outdoor photography with vivid colors and dramatic lighting',
        thumbnail: '🏔️',
        category: 'landscape',
        tags: ['mountains', 'nature', 'vivid', 'dramatic', 'outdoor'],
        presets: ['Vivid Landscape', 'Moody Mountains', 'Golden Hour'],
        exportSettings: {
          format: 'JPEG',
          resolution: '4K',
          quality: 90,
          includeMetadata: true,
        },
        metadata: {
          camera: 'Canon EOS R5',
          location: 'Rocky Mountains',
          style: 'Vivid and dramatic',
        },
        usageCount: 127,
        isFavorite: true,
        createdAt: '2024-01-15T10:30:00Z',
        createdBy: 'Sarah Chen',
      },
      {
        id: '2',
        name: 'Urban Street',
        description: 'Modern city photography with high contrast and urban aesthetics',
        thumbnail: '🏙️',
        category: 'street',
        tags: ['urban', 'city', 'street', 'modern', 'contrast'],
        presets: ['Urban Contrast', 'Street Noir', 'City Lights'],
        exportSettings: {
          format: 'JPEG',
          resolution: '2K',
          quality: 85,
          includeMetadata: true,
        },
        metadata: {
          camera: 'Sony A7R IV',
          location: 'New York',
          style: 'High contrast urban',
        },
        usageCount: 89,
        isFavorite: false,
        createdAt: '2024-01-14T15:20:00Z',
        createdBy: 'Marcus Rodriguez',
      },
      {
        id: '3',
        name: 'Wedding Collection',
        description: 'Elegant wedding photography with soft, romantic tones',
        thumbnail: '💒',
        category: 'wedding',
        tags: ['wedding', 'romantic', 'soft', 'elegant', 'portrait'],
        presets: ['Wedding Soft', 'Romantic Glow', 'Elegant Portrait'],
        exportSettings: {
          format: 'JPEG',
          resolution: 'Original',
          quality: 95,
          includeMetadata: true,
        },
        metadata: {
          camera: 'Nikon Z9',
          location: 'Various',
          style: 'Soft and romantic',
        },
        usageCount: 234,
        isFavorite: true,
        createdAt: '2024-01-13T09:15:00Z',
        createdBy: 'Emma Thompson',
      },
      {
        id: '4',
        name: 'Commercial Product',
        description: 'Professional product photography with clean, studio lighting',
        thumbnail: '📦',
        category: 'commercial',
        tags: ['commercial', 'product', 'studio', 'clean', 'professional'],
        presets: ['Studio Clean', 'Product Sharp', 'Commercial Bright'],
        exportSettings: {
          format: 'TIFF',
          resolution: 'Original',
          quality: 100,
          includeMetadata: true,
        },
        metadata: {
          camera: 'Fujifilm GFX 100S',
          location: 'Studio',
          style: 'Clean and professional',
        },
        usageCount: 67,
        isFavorite: false,
        createdAt: '2024-01-12T14:45:00Z',
        createdBy: 'David Kim',
      },
    ];
    
    setTemplates(mockTemplates);
  };

  const handleUseTemplate = (template: ProjectTemplate) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Medium);
    onUseTemplate(template);
    onClose();
  };

  const handleSaveTemplate = () => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Medium);
    // This would typically open a form to create a new template
    Alert.alert('Save Template', 'Template saving functionality will be implemented');
  };

  const handleFavoriteToggle = (templateId: string) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    setTemplates(prev => 
      prev.map(template => 
        template.id === templateId 
          ? { ...template, isFavorite: !template.isFavorite }
          : template
      )
    );
  };

  const handleCategoryChange = (category: typeof selectedCategory) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    setSelectedCategory(category);
  };

  const handleTemplatePress = (template: ProjectTemplate) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    setSelectedTemplate(template);
    setShowDetails(true);
  };

  const filteredTemplates = templates.filter(template => {
    const matchesSearch = 
      template.name.toLowerCase().includes(searchQuery.toLowerCase()) ||
      template.description.toLowerCase().includes(searchQuery.toLowerCase()) ||
      template.tags.some(tag => tag.toLowerCase().includes(searchQuery.toLowerCase()));
    
    const matchesCategory = selectedCategory === 'all' || template.category === selectedCategory;
    
    return matchesSearch && matchesCategory;
  });

  const categories = [
    { key: 'all', label: 'All', icon: 'grid-outline', color: theme.colors.text.primary },
    { key: 'landscape', label: 'Landscape', icon: 'mountain-outline', color: theme.colors.system.green },
    { key: 'portrait', label: 'Portrait', icon: 'person-outline', color: theme.colors.system.blue },
    { key: 'street', label: 'Street', icon: 'car-outline', color: theme.colors.system.orange },
    { key: 'wedding', label: 'Wedding', icon: 'heart-outline', color: theme.colors.system.pink },
    { key: 'commercial', label: 'Commercial', icon: 'business-outline', color: theme.colors.system.purple },
    { key: 'custom', label: 'Custom', icon: 'settings-outline', color: theme.colors.system.teal },
  ];

  const getCategoryIcon = (category: ProjectTemplate['category']) => {
    switch (category) {
      case 'landscape': return 'mountain-outline';
      case 'portrait': return 'person-outline';
      case 'street': return 'car-outline';
      case 'wedding': return 'heart-outline';
      case 'commercial': return 'business-outline';
      case 'custom': return 'settings-outline';
      default: return 'image-outline';
    }
  };

  if (!isVisible) return null;

  return (
    <View style={styles.overlay}>
      <TouchableOpacity style={styles.backdrop} onPress={onClose} />
      <Animated.View 
        style={[
          styles.container,
          { 
            transform: [{ translateX: slideAnim }],
            opacity: fadeAnim,
          }
        ]}
      >
        {/* Header */}
        <View style={styles.header}>
          <View style={styles.headerLeft}>
            <Text style={styles.title}>Templates</Text>
            <View style={styles.templateCount}>
              <Text style={styles.templateCountText}>
                {templates.length} templates
              </Text>
            </View>
          </View>
          <TouchableOpacity onPress={onClose} style={styles.closeButton}>
            <Ionicons name="close" size={24} color={theme.colors.text.secondary} />
          </TouchableOpacity>
        </View>

        {/* Search Bar */}
        <View style={styles.searchContainer}>
          <View style={styles.searchBar}>
            <Ionicons name="search" size={20} color={theme.colors.text.secondary} />
            <TextInput
              style={styles.searchInput}
              placeholder="Search templates..."
              placeholderTextColor={theme.colors.text.tertiary}
              value={searchQuery}
              onChangeText={setSearchQuery}
            />
          </View>
        </View>

        {/* Category Filters */}
        <ScrollView 
          horizontal 
          showsHorizontalScrollIndicator={false}
          contentContainerStyle={styles.categoryContainer}
        >
          {categories.map((category) => (
            <TouchableOpacity
              key={category.key}
              style={[
                styles.categoryTab,
                selectedCategory === category.key && styles.categoryTabActive
              ]}
              onPress={() => handleCategoryChange(category.key as typeof selectedCategory)}
            >
              <Ionicons 
                name={category.icon as keyof typeof Ionicons.glyphMap} 
                size={16} 
                color={selectedCategory === category.key ? theme.colors.surface : category.color} 
              />
              <Text style={[
                styles.categoryText,
                selectedCategory === category.key && styles.categoryTextActive
              ]}>
                {category.label}
              </Text>
            </TouchableOpacity>
          ))}
        </ScrollView>

        {/* Create Template Button */}
        <View style={styles.createSection}>
          <TouchableOpacity 
            style={styles.createButton}
            onPress={handleSaveTemplate}
          >
            <Ionicons name="add-circle-outline" size={20} color={theme.colors.green} />
            <Text style={styles.createButtonText}>Create New Template</Text>
          </TouchableOpacity>
        </View>

        {/* Templates List */}
        <ScrollView style={styles.templatesContainer} showsVerticalScrollIndicator={false}>
          {filteredTemplates.map((template) => (
            <TouchableOpacity
              key={template.id}
              style={styles.templateCard}
              onPress={() => handleTemplatePress(template)}
              activeOpacity={0.8}
            >
              {/* Template Thumbnail */}
              <View style={styles.thumbnailContainer}>
                <Text style={styles.thumbnail}>{template.thumbnail}</Text>
                <TouchableOpacity 
                  style={styles.favoriteButton}
                  onPress={() => handleFavoriteToggle(template.id)}
                >
                  <Ionicons 
                    name={template.isFavorite ? "heart" : "heart-outline"} 
                    size={20} 
                    color={template.isFavorite ? theme.colors.system.red : theme.colors.text.tertiary} 
                  />
                </TouchableOpacity>
              </View>

              {/* Template Info */}
              <View style={styles.templateInfo}>
                <View style={styles.templateHeader}>
                  <Text style={styles.templateName} numberOfLines={1}>
                    {template.name}
                  </Text>
                  <View style={styles.usageBadge}>
                    <Text style={styles.usageText}>{template.usageCount}</Text>
                  </View>
                </View>
                
                <Text style={styles.templateDescription} numberOfLines={2}>
                  {template.description}
                </Text>
                
                {/* Tags */}
                <View style={styles.tagsContainer}>
                  {template.tags.slice(0, 3).map((tag, index) => (
                    <View key={index} style={styles.tagChip}>
                      <Text style={styles.tagText}>#{tag}</Text>
                    </View>
                  ))}
                  {template.tags.length > 3 && (
                    <Text style={styles.moreTagsText}>+{template.tags.length - 3}</Text>
                  )}
                </View>
                
                {/* Presets */}
                <View style={styles.presetsContainer}>
                  <Text style={styles.presetsLabel}>Presets:</Text>
                  <Text style={styles.presetsText} numberOfLines={1}>
                    {template.presets.join(', ')}
                  </Text>
                </View>
                
                {/* Metadata */}
                <View style={styles.metadataRow}>
                  {template.metadata.camera && (
                    <View style={styles.metadataItem}>
                      <Ionicons name="camera-outline" size={12} color={theme.colors.text.tertiary} />
                      <Text style={styles.metadataText}>{template.metadata.camera}</Text>
                    </View>
                  )}
                  
                  {template.metadata.location && (
                    <View style={styles.metadataItem}>
                      <Ionicons name="location-outline" size={12} color={theme.colors.text.tertiary} />
                      <Text style={styles.metadataText}>{template.metadata.location}</Text>
                    </View>
                  )}
                </View>
              </View>

              {/* Action Button */}
              <TouchableOpacity 
                style={styles.useButton}
                onPress={() => handleUseTemplate(template)}
              >
                <Ionicons name="play" size={20} color={theme.colors.surface} />
              </TouchableOpacity>
            </TouchableOpacity>
          ))}
        </ScrollView>

        {/* Template Details Modal */}
        {showDetails && selectedTemplate && (
          <View style={styles.detailsOverlay}>
            <View style={styles.detailsContainer}>
              <View style={styles.detailsHeader}>
                <Text style={styles.detailsTitle}>{selectedTemplate.name}</Text>
                <TouchableOpacity onPress={() => setShowDetails(false)}>
                  <Ionicons name="close" size={24} color={theme.colors.text.secondary} />
                </TouchableOpacity>
              </View>
              
              <ScrollView style={styles.detailsContent} showsVerticalScrollIndicator={false}>
                <View style={styles.detailsThumbnail}>
                  <Text style={styles.detailsThumbnailText}>{selectedTemplate.thumbnail}</Text>
                </View>
                
                <Text style={styles.detailsDescription}>
                  {selectedTemplate.description}
                </Text>
                
                <View style={styles.detailsSection}>
                  <Text style={styles.detailsSectionTitle}>Tags</Text>
                  <View style={styles.detailsTags}>
                    {selectedTemplate.tags.map((tag, index) => (
                      <View key={index} style={styles.detailsTag}>
                        <Text style={styles.detailsTagText}>#{tag}</Text>
                      </View>
                    ))}
                  </View>
                </View>
                
                <View style={styles.detailsSection}>
                  <Text style={styles.detailsSectionTitle}>Presets</Text>
                  {selectedTemplate.presets.map((preset, index) => (
                    <View key={index} style={styles.presetItem}>
                      <Ionicons name="brush-outline" size={16} color={theme.colors.green} />
                      <Text style={styles.presetText}>{preset}</Text>
                    </View>
                  ))}
                </View>
                
                <View style={styles.detailsSection}>
                  <Text style={styles.detailsSectionTitle}>Export Settings</Text>
                  <View style={styles.exportSettings}>
                    <Text style={styles.exportSettingText}>
                      Format: {selectedTemplate.exportSettings.format}
                    </Text>
                    <Text style={styles.exportSettingText}>
                      Resolution: {selectedTemplate.exportSettings.resolution}
                    </Text>
                    <Text style={styles.exportSettingText}>
                      Quality: {selectedTemplate.exportSettings.quality}%
                    </Text>
                    <Text style={styles.exportSettingText}>
                      Metadata: {selectedTemplate.exportSettings.includeMetadata ? 'Yes' : 'No'}
                    </Text>
                  </View>
                </View>
                
                <View style={styles.detailsSection}>
                  <Text style={styles.detailsSectionTitle}>Created by</Text>
                  <Text style={styles.createdByText}>{selectedTemplate.createdBy}</Text>
                  <Text style={styles.createdDateText}>
                    {new Date(selectedTemplate.createdAt).toLocaleDateString()}
                  </Text>
                </View>
              </ScrollView>
              
              <View style={styles.detailsActions}>
                <TouchableOpacity 
                  style={styles.detailsUseButton}
                  onPress={() => handleUseTemplate(selectedTemplate)}
                >
                  <Ionicons name="play" size={20} color={theme.colors.surface} />
                  <Text style={styles.detailsUseButtonText}>Use This Template</Text>
                </TouchableOpacity>
              </View>
            </View>
          </View>
        )}
      </Animated.View>
    </View>
  );
};

const styles = StyleSheet.create({
  overlay: {
    position: 'absolute',
    top: 0,
    left: 0,
    right: 0,
    bottom: 0,
    zIndex: 1000,
  },
  backdrop: {
    position: 'absolute',
    top: 0,
    left: 0,
    right: 0,
    bottom: 0,
    backgroundColor: 'rgba(0, 0, 0, 0.3)',
  },
  container: {
    position: 'absolute',
    top: 0,
    right: 0,
    bottom: 0,
    width: 400,
    backgroundColor: theme.colors.surface,
    ...theme.shadows.large,
  },
  header: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    padding: theme.spacing.l,
    borderBottomWidth: 1,
    borderBottomColor: theme.colors.border,
  },
  headerLeft: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: theme.spacing.s,
  },
  title: {
    fontSize: theme.fontSizes.title,
    fontWeight: '700',
    color: theme.colors.text.primary,
  },
  templateCount: {
    backgroundColor: theme.colors.lightGray,
    paddingHorizontal: theme.spacing.xs,
    paddingVertical: 2,
    borderRadius: theme.borderRadius.s,
  },
  templateCountText: {
    fontSize: theme.fontSizes.caption,
    color: theme.colors.text.secondary,
    fontWeight: '600',
  },
  closeButton: {
    padding: theme.spacing.s,
  },
  searchContainer: {
    padding: theme.spacing.l,
    paddingTop: theme.spacing.m,
  },
  searchBar: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: theme.colors.lightGray,
    borderRadius: theme.borderRadius.m,
    paddingHorizontal: theme.spacing.m,
    paddingVertical: theme.spacing.s,
  },
  searchInput: {
    flex: 1,
    marginLeft: theme.spacing.s,
    fontSize: theme.fontSizes.body,
    color: theme.colors.text.primary,
  },
  categoryContainer: {
    paddingHorizontal: theme.spacing.l,
    paddingBottom: theme.spacing.m,
  },
  categoryTab: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: theme.spacing.m,
    paddingVertical: theme.spacing.s,
    marginRight: theme.spacing.s,
    backgroundColor: theme.colors.lightGray,
    borderRadius: theme.borderRadius.m,
    gap: theme.spacing.xs,
  },
  categoryTabActive: {
    backgroundColor: theme.colors.green,
  },
  categoryText: {
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.text.secondary,
  },
  categoryTextActive: {
    color: theme.colors.surface,
  },
  createSection: {
    paddingHorizontal: theme.spacing.l,
    paddingBottom: theme.spacing.m,
  },
  createButton: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'center',
    paddingVertical: theme.spacing.m,
    paddingHorizontal: theme.spacing.l,
    backgroundColor: theme.colors.green + '10',
    borderRadius: theme.borderRadius.m,
    borderWidth: 2,
    borderColor: theme.colors.green,
    gap: theme.spacing.s,
  },
  createButtonText: {
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.green,
  },
  templatesContainer: {
    flex: 1,
    paddingHorizontal: theme.spacing.l,
  },
  templateCard: {
    flexDirection: 'row',
    backgroundColor: theme.colors.lightGray,
    borderRadius: theme.borderRadius.m,
    padding: theme.spacing.m,
    marginBottom: theme.spacing.s,
    borderWidth: 2,
    borderColor: 'transparent',
  },
  thumbnailContainer: {
    position: 'relative',
    marginRight: theme.spacing.m,
  },
  thumbnail: {
    fontSize: 48,
    width: 60,
    height: 60,
    textAlign: 'center',
    lineHeight: 60,
    backgroundColor: theme.colors.surface,
    borderRadius: theme.borderRadius.m,
    ...theme.shadows.subtle,
  },
  favoriteButton: {
    position: 'absolute',
    top: -8,
    right: -8,
    padding: 4,
    backgroundColor: theme.colors.surface,
    borderRadius: 12,
    ...theme.shadows.subtle,
  },
  templateInfo: {
    flex: 1,
  },
  templateHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: theme.spacing.xs,
  },
  templateName: {
    fontSize: theme.fontSizes.headline,
    fontWeight: '700',
    color: theme.colors.text.primary,
    flex: 1,
    marginRight: theme.spacing.s,
  },
  usageBadge: {
    backgroundColor: theme.colors.green,
    paddingHorizontal: theme.spacing.xs,
    paddingVertical: 2,
    borderRadius: theme.borderRadius.s,
    minWidth: 24,
    alignItems: 'center',
  },
  usageText: {
    fontSize: theme.fontSizes.caption,
    fontWeight: '700',
    color: theme.colors.surface,
  },
  templateDescription: {
    fontSize: theme.fontSizes.body,
    color: theme.colors.text.secondary,
    marginBottom: theme.spacing.xs,
    lineHeight: 18,
  },
  tagsContainer: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: theme.spacing.xs,
    marginBottom: theme.spacing.xs,
  },
  tagChip: {
    backgroundColor: theme.colors.surface,
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
  presetsContainer: {
    marginBottom: theme.spacing.xs,
  },
  presetsLabel: {
    fontSize: theme.fontSizes.caption,
    fontWeight: '600',
    color: theme.colors.text.secondary,
    marginBottom: 2,
  },
  presetsText: {
    fontSize: theme.fontSizes.caption,
    color: theme.colors.text.primary,
    fontWeight: '500',
  },
  metadataRow: {
    flexDirection: 'row',
    gap: theme.spacing.m,
  },
  metadataItem: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: theme.spacing.xs,
  },
  metadataText: {
    fontSize: theme.fontSizes.caption,
    color: theme.colors.text.tertiary,
    fontWeight: '500',
  },
  useButton: {
    width: 48,
    height: 48,
    backgroundColor: theme.colors.green,
    borderRadius: 24,
    justifyContent: 'center',
    alignItems: 'center',
    alignSelf: 'center',
    ...theme.shadows.subtle,
  },
  detailsOverlay: {
    position: 'absolute',
    top: 0,
    left: 0,
    right: 0,
    bottom: 0,
    backgroundColor: 'rgba(0, 0, 0, 0.8)',
    justifyContent: 'center',
    alignItems: 'center',
    zIndex: 1001,
  },
  detailsContainer: {
    backgroundColor: theme.colors.surface,
    borderRadius: theme.borderRadius.xl,
    width: '90%',
    maxHeight: '80%',
    ...theme.shadows.large,
  },
  detailsHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    padding: theme.spacing.l,
    borderBottomWidth: 1,
    borderBottomColor: theme.colors.border,
  },
  detailsTitle: {
    fontSize: theme.fontSizes.title,
    fontWeight: '700',
    color: theme.colors.text.primary,
  },
  detailsContent: {
    padding: theme.spacing.l,
  },
  detailsThumbnail: {
    alignItems: 'center',
    marginBottom: theme.spacing.l,
  },
  detailsThumbnailText: {
    fontSize: 80,
    textAlign: 'center',
    backgroundColor: theme.colors.lightGray,
    width: 120,
    height: 120,
    borderRadius: theme.borderRadius.l,
    lineHeight: 120,
  },
  detailsDescription: {
    fontSize: theme.fontSizes.body,
    color: theme.colors.text.primary,
    lineHeight: 20,
    marginBottom: theme.spacing.l,
    textAlign: 'center',
  },
  detailsSection: {
    marginBottom: theme.spacing.l,
  },
  detailsSectionTitle: {
    fontSize: theme.fontSizes.headline,
    fontWeight: '700',
    color: theme.colors.text.primary,
    marginBottom: theme.spacing.s,
  },
  detailsTags: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: theme.spacing.xs,
  },
  detailsTag: {
    backgroundColor: theme.colors.lightGray,
    paddingHorizontal: theme.spacing.m,
    paddingVertical: theme.spacing.xs,
    borderRadius: theme.borderRadius.m,
  },
  detailsTagText: {
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.text.primary,
  },
  presetItem: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: theme.spacing.s,
    paddingVertical: theme.spacing.xs,
  },
  presetText: {
    fontSize: theme.fontSizes.body,
    color: theme.colors.text.primary,
    fontWeight: '500',
  },
  exportSettings: {
    gap: theme.spacing.xs,
  },
  exportSettingText: {
    fontSize: theme.fontSizes.body,
    color: theme.colors.text.primary,
    fontWeight: '500',
  },
  createdByText: {
    fontSize: theme.fontSizes.body,
    color: theme.colors.text.primary,
    fontWeight: '600',
    marginBottom: 2,
  },
  createdDateText: {
    fontSize: theme.fontSizes.caption,
    color: theme.colors.text.secondary,
  },
  detailsActions: {
    padding: theme.spacing.l,
    borderTopWidth: 1,
    borderTopColor: theme.colors.border,
  },
  detailsUseButton: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'center',
    paddingVertical: theme.spacing.m,
    paddingHorizontal: theme.spacing.l,
    backgroundColor: theme.colors.green,
    borderRadius: theme.borderRadius.m,
    gap: theme.spacing.s,
  },
  detailsUseButtonText: {
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.surface,
  },
});

export default TemplateManager;
