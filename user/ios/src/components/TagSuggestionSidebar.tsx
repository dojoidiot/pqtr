import React, { useState, useEffect } from 'react';
import {
  View,
  Text,
  TouchableOpacity,
  StyleSheet,
  ScrollView,
  TextInput,
  Animated,
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import * as Haptics from 'expo-haptics';
import { theme } from '../constants/theme';

interface TagSuggestion {
  id: string;
  name: string;
  confidence: number;
  category: 'location' | 'subject' | 'style' | 'camera' | 'time';
  usageCount: number;
  isSuggested: boolean;
}

interface TagSuggestionSidebarProps {
  isVisible: boolean;
  onClose: () => void;
  selectedImage?: {
    id: string;
    tags: string[];
    metadata?: {
      camera?: string;
      location?: string;
      timestamp?: string;
    };
  };
  onTagsUpdate: (tags: string[]) => void;
}

const TagSuggestionSidebar: React.FC<TagSuggestionSidebarProps> = ({
  isVisible,
  onClose,
  selectedImage,
  onTagsUpdate,
}) => {
  const [searchQuery, setSearchQuery] = useState('');
  const [selectedCategory, setSelectedCategory] = useState<'all' | TagSuggestion['category']>('all');
  const [selectedTags, setSelectedTags] = useState<string[]>(selectedImage?.tags || []);
  const [suggestions, setSuggestions] = useState<TagSuggestion[]>([]);
  const [isBatchMode, setIsBatchMode] = useState(false);
  const slideAnim = useState(new Animated.Value(300))[0];

  useEffect(() => {
    if (isVisible) {
      Animated.spring(slideAnim, {
        toValue: 0,
        useNativeDriver: true,
        tension: 100,
        friction: 8,
      }).start();
      loadSuggestions();
    } else {
      Animated.spring(slideAnim, {
        toValue: 300,
        useNativeDriver: true,
        tension: 100,
        friction: 8,
      }).start();
    }
  }, [isVisible]);

  useEffect(() => {
    if (selectedImage) {
      setSelectedTags(selectedImage.tags);
    }
  }, [selectedImage]);

  const loadSuggestions = () => {
    // Mock ML suggestions based on image metadata
    const mockSuggestions: TagSuggestion[] = [
      // Location-based suggestions
      { id: '1', name: 'mountains', confidence: 0.95, category: 'location', usageCount: 127, isSuggested: true },
      { id: '2', name: 'forest', confidence: 0.87, category: 'location', usageCount: 89, isSuggested: true },
      { id: '3', name: 'urban', confidence: 0.76, category: 'location', usageCount: 156, isSuggested: false },
      { id: '4', name: 'coast', confidence: 0.68, category: 'location', usageCount: 73, isSuggested: false },
      
      // Subject-based suggestions
      { id: '5', name: 'landscape', confidence: 0.92, category: 'subject', usageCount: 234, isSuggested: true },
      { id: '6', name: 'architecture', confidence: 0.81, category: 'subject', usageCount: 98, isSuggested: false },
      { id: '7', name: 'portrait', confidence: 0.45, category: 'subject', usageCount: 67, isSuggested: false },
      { id: '8', name: 'street', confidence: 0.72, category: 'subject', usageCount: 112, isSuggested: false },
      
      // Style-based suggestions
      { id: '9', name: 'vivid', confidence: 0.88, category: 'style', usageCount: 145, isSuggested: true },
      { id: '10', name: 'moody', confidence: 0.79, category: 'style', usageCount: 89, isSuggested: false },
      { id: '11', name: 'minimal', confidence: 0.65, category: 'style', usageCount: 56, isSuggested: false },
      { id: '12', name: 'dramatic', confidence: 0.71, category: 'style', usageCount: 78, isSuggested: false },
      
      // Camera-based suggestions
      { id: '13', name: 'canon', confidence: 0.82, category: 'camera', usageCount: 234, isSuggested: true },
      { id: '14', name: 'sony', confidence: 0.68, category: 'camera', usageCount: 156, isSuggested: false },
      { id: '15', name: 'nikon', confidence: 0.59, category: 'camera', usageCount: 98, isSuggested: false },
      
      // Time-based suggestions
      { id: '16', name: 'golden-hour', confidence: 0.91, category: 'time', usageCount: 189, isSuggested: true },
      { id: '17', name: 'blue-hour', confidence: 0.74, category: 'time', usageCount: 67, isSuggested: false },
      { id: '18', name: 'midday', confidence: 0.45, category: 'time', usageCount: 45, isSuggested: false },
    ];

    // Filter based on selected image metadata
    if (selectedImage?.metadata) {
      const { camera, location, timestamp } = selectedImage.metadata;
      
      if (camera) {
        mockSuggestions.forEach(suggestion => {
          if (suggestion.category === 'camera' && suggestion.name.toLowerCase().includes(camera.toLowerCase())) {
            suggestion.confidence += 0.2;
            suggestion.isSuggested = true;
          }
        });
      }
      
      if (location) {
        mockSuggestions.forEach(suggestion => {
          if (suggestion.category === 'location' && suggestion.name.toLowerCase().includes(location.toLowerCase())) {
            suggestion.confidence += 0.15;
            suggestion.isSuggested = true;
          }
        });
      }
      
      if (timestamp) {
        const hour = new Date(timestamp).getHours();
        if (hour >= 16 || hour <= 8) {
          mockSuggestions.forEach(suggestion => {
            if (suggestion.name === 'golden-hour') {
              suggestion.confidence += 0.25;
              suggestion.isSuggested = true;
            }
          });
        }
      }
    }

    setSuggestions(mockSuggestions.sort((a, b) => b.confidence - a.confidence));
  };

  const handleTagToggle = (tagName: string) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    
    setSelectedTags(prev => {
      if (prev.includes(tagName)) {
        return prev.filter(tag => tag !== tagName);
      } else {
        return [...prev, tagName];
      }
    });
  };

  const handleApplyTags = () => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Medium);
    onTagsUpdate(selectedTags);
    onClose();
  };

  const handleBatchModeToggle = () => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    setIsBatchMode(!isBatchMode);
  };

  const filteredSuggestions = suggestions.filter(suggestion => {
    const matchesSearch = suggestion.name.toLowerCase().includes(searchQuery.toLowerCase());
    const matchesCategory = selectedCategory === 'all' || suggestion.category === selectedCategory;
    return matchesSearch && matchesCategory;
  });

  const categories = [
    { key: 'all', label: 'All', icon: 'grid-outline', color: theme.colors.text.primary },
    { key: 'location', label: 'Location', icon: 'location-outline', color: theme.colors.system.blue },
    { key: 'subject', label: 'Subject', icon: 'image-outline', color: theme.colors.system.green },
    { key: 'style', label: 'Style', icon: 'brush-outline', color: theme.colors.system.orange },
    { key: 'camera', label: 'Camera', icon: 'camera-outline', color: theme.colors.system.purple },
    { key: 'time', label: 'Time', icon: 'time-outline', color: theme.colors.system.teal },
  ];

  if (!isVisible) return null;

  return (
    <View style={styles.overlay}>
      <TouchableOpacity style={styles.backdrop} onPress={onClose} />
      <Animated.View style={[styles.sidebar, { transform: [{ translateX: slideAnim }] }]}>
        {/* Header */}
        <View style={styles.header}>
          <View style={styles.headerLeft}>
            <Text style={styles.title}>Smart Tags</Text>
            <View style={styles.suggestedBadge}>
              <Ionicons name="sparkles" size={12} color={theme.colors.surface} />
              <Text style={styles.suggestedText}>AI Suggested</Text>
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
              placeholder="Search tags..."
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
              onPress={() => setSelectedCategory(category.key as typeof selectedCategory)}
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

        {/* Batch Mode Toggle */}
        <View style={styles.batchToggle}>
          <Text style={styles.batchLabel}>Batch Edit Mode</Text>
          <TouchableOpacity
            style={[
              styles.batchToggleButton,
              isBatchMode && styles.batchToggleButtonActive
            ]}
            onPress={handleBatchModeToggle}
          >
            <View style={[
              styles.batchToggleThumb,
              isBatchMode && styles.batchToggleThumbActive
            ]} />
          </TouchableOpacity>
        </View>

        {/* Tag Suggestions */}
        <ScrollView style={styles.suggestionsContainer} showsVerticalScrollIndicator={false}>
          {filteredSuggestions.map((suggestion) => (
            <TouchableOpacity
              key={suggestion.id}
              style={[
                styles.suggestionItem,
                selectedTags.includes(suggestion.name) && styles.suggestionItemSelected
              ]}
              onPress={() => handleTagToggle(suggestion.name)}
            >
              <View style={styles.suggestionLeft}>
                <View style={styles.tagInfo}>
                  <Text style={[
                    styles.tagName,
                    selectedTags.includes(suggestion.name) && styles.tagNameSelected
                  ]}>
                    #{suggestion.name}
                  </Text>
                  {suggestion.isSuggested && (
                    <View style={styles.aiBadge}>
                      <Ionicons name="sparkles" size={10} color={theme.colors.system.yellow} />
                    </View>
                  )}
                </View>
                <View style={styles.tagMeta}>
                  <Text style={styles.confidenceText}>
                    {Math.round(suggestion.confidence * 100)}% match
                  </Text>
                  <Text style={styles.usageText}>
                    {suggestion.usageCount} uses
                  </Text>
                </View>
              </View>
              
              <View style={styles.suggestionRight}>
                <View style={[
                  styles.categoryIndicator,
                  { backgroundColor: getCategoryColor(suggestion.category) }
                ]} />
                <Ionicons 
                  name={selectedTags.includes(suggestion.name) ? "checkmark-circle" : "add-circle-outline"} 
                  size={24} 
                  color={selectedTags.includes(suggestion.name) ? theme.colors.green : theme.colors.text.tertiary} 
                />
              </View>
            </TouchableOpacity>
          ))}
        </ScrollView>

        {/* Action Buttons */}
        <View style={styles.actions}>
          <TouchableOpacity style={styles.cancelButton} onPress={onClose}>
            <Text style={styles.cancelButtonText}>Cancel</Text>
          </TouchableOpacity>
          <TouchableOpacity style={styles.applyButton} onPress={handleApplyTags}>
            <Ionicons name="checkmark" size={20} color={theme.colors.surface} />
            <Text style={styles.applyButtonText}>
              Apply ({selectedTags.length})
            </Text>
          </TouchableOpacity>
        </View>
      </Animated.View>
    </View>
  );
};

const getCategoryColor = (category: TagSuggestion['category']) => {
  switch (category) {
    case 'location': return theme.colors.system.blue;
    case 'subject': return theme.colors.system.green;
    case 'style': return theme.colors.system.orange;
    case 'camera': return theme.colors.system.purple;
    case 'time': return theme.colors.system.teal;
    default: return theme.colors.gray;
  }
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
  sidebar: {
    position: 'absolute',
    top: 0,
    right: 0,
    bottom: 0,
    width: 320,
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
  suggestedBadge: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: theme.colors.green,
    paddingHorizontal: theme.spacing.xs,
    paddingVertical: 2,
    borderRadius: theme.borderRadius.s,
    gap: 2,
  },
  suggestedText: {
    fontSize: theme.fontSizes.caption,
    color: theme.colors.surface,
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
  batchToggle: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingHorizontal: theme.spacing.l,
    paddingBottom: theme.spacing.m,
  },
  batchLabel: {
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.text.primary,
  },
  batchToggleButton: {
    width: 44,
    height: 24,
    backgroundColor: theme.colors.lightGray,
    borderRadius: 12,
    padding: 2,
  },
  batchToggleButtonActive: {
    backgroundColor: theme.colors.green,
  },
  batchToggleThumb: {
    width: 20,
    height: 20,
    backgroundColor: theme.colors.surface,
    borderRadius: 10,
    transform: [{ translateX: 0 }],
  },
  batchToggleThumbActive: {
    transform: [{ translateX: 20 }],
  },
  suggestionsContainer: {
    flex: 1,
    paddingHorizontal: theme.spacing.l,
  },
  suggestionItem: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    paddingVertical: theme.spacing.m,
    paddingHorizontal: theme.spacing.m,
    backgroundColor: theme.colors.lightGray,
    borderRadius: theme.borderRadius.m,
    marginBottom: theme.spacing.s,
    borderWidth: 2,
    borderColor: 'transparent',
  },
  suggestionItemSelected: {
    borderColor: theme.colors.green,
    backgroundColor: theme.colors.green + '10',
  },
  suggestionLeft: {
    flex: 1,
  },
  tagInfo: {
    flexDirection: 'row',
    alignItems: 'center',
    marginBottom: theme.spacing.xs,
    gap: theme.spacing.xs,
  },
  tagName: {
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.text.primary,
  },
  tagNameSelected: {
    color: theme.colors.green,
  },
  aiBadge: {
    backgroundColor: theme.colors.system.yellow + '20',
    padding: 2,
    borderRadius: 4,
  },
  tagMeta: {
    flexDirection: 'row',
    gap: theme.spacing.m,
  },
  confidenceText: {
    fontSize: theme.fontSizes.caption,
    color: theme.colors.text.secondary,
    fontWeight: '500',
  },
  usageText: {
    fontSize: theme.fontSizes.caption,
    color: theme.colors.text.tertiary,
    fontWeight: '500',
  },
  suggestionRight: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: theme.spacing.s,
  },
  categoryIndicator: {
    width: 8,
    height: 8,
    borderRadius: 4,
  },
  actions: {
    flexDirection: 'row',
    padding: theme.spacing.l,
    gap: theme.spacing.m,
    borderTopWidth: 1,
    borderTopColor: theme.colors.border,
  },
  cancelButton: {
    flex: 1,
    paddingVertical: theme.spacing.m,
    paddingHorizontal: theme.spacing.l,
    backgroundColor: theme.colors.lightGray,
    borderRadius: theme.borderRadius.m,
    alignItems: 'center',
  },
  cancelButtonText: {
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.text.secondary,
  },
  applyButton: {
    flex: 2,
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'center',
    paddingVertical: theme.spacing.m,
    paddingHorizontal: theme.spacing.l,
    backgroundColor: theme.colors.green,
    borderRadius: theme.borderRadius.m,
    gap: theme.spacing.s,
  },
  applyButtonText: {
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.surface,
  },
});

export default TagSuggestionSidebar;
