import React, { useState, useRef, useEffect } from 'react';
import {
  SafeAreaView,
  Text,
  View,
  TouchableOpacity,
  StyleSheet,
  TextInput,
  ScrollView,
  FlatList,
  Image,
  Animated,
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import * as Haptics from 'expo-haptics';
import { theme } from '../constants/theme';

interface SearchResult {
  id: string;
  imageUrl: string;
  thumbnail: string;
  title: string;
  tags: string[];
  presetApplied?: string;
  camera?: string;
  location?: string;
  timestamp: string;
  filename: string;
  size: string;
}

interface SearchFilter {
  type: 'tag' | 'preset' | 'location' | 'camera' | 'date';
  value: string;
  label: string;
}

const mockSearchResults: SearchResult[] = [
  {
    id: '1',
    imageUrl: 'https://images.unsplash.com/photo-1506905925346-21bda4d32df4?w=400',
    thumbnail: 'https://images.unsplash.com/photo-1506905925346-21bda4d32df4?w=100',
    title: 'Mountain Landscape',
    tags: ['landscape', 'mountains', 'nature', 'vivid'],
    presetApplied: 'Vivid Landscape',
    camera: 'Canon EOS R5',
    location: 'Rocky Mountains, CO',
    timestamp: '2024-01-15T10:30:00Z',
    filename: 'IMG_001.jpg',
    size: '24.5 MB',
  },
  {
    id: '2',
    imageUrl: 'https://images.unsplash.com/photo-1441974231531-c6227db76b6e?w=400',
    thumbnail: 'https://images.unsplash.com/photo-1441974231531-c6227db76b6e?w=100',
    title: 'Forest Path',
    tags: ['forest', 'path', 'green', 'moody'],
    presetApplied: 'Moody Forest',
    camera: 'Sony A7R IV',
    location: 'Redwood Forest, CA',
    timestamp: '2024-01-15T09:15:00Z',
    filename: 'IMG_002.jpg',
    size: '18.2 MB',
  },
  {
    id: '3',
    imageUrl: 'https://images.unsplash.com/photo-1506905925346-21bda4d32df4?w=400',
    thumbnail: 'https://images.unsplash.com/photo-1506905925346-21bda4d32df4?w=100',
    title: 'City Skyline',
    tags: ['city', 'skyline', 'urban', 'dramatic'],
    presetApplied: 'Urban Contrast',
    camera: 'Nikon Z9',
    location: 'New York, NY',
    timestamp: '2024-01-15T08:45:00Z',
    filename: 'IMG_003.jpg',
    size: '22.8 MB',
  },
];

const SearchScreen: React.FC = () => {
  const [searchQuery, setSearchQuery] = useState('');
  const [activeFilters, setActiveFilters] = useState<SearchFilter[]>([]);
  const [searchResults, setSearchResults] = useState<SearchResult[]>([]);
  const [isSearching, setIsSearching] = useState(false);
  const [sortBy, setSortBy] = useState<'relevance' | 'time' | 'camera' | 'preset'>('relevance');
  const [showSuggestions, setShowSuggestions] = useState(false);
  const searchInputRef = useRef<TextInput>(null);
  const fadeAnim = useRef(new Animated.Value(0)).current;

  useEffect(() => {
    Animated.timing(fadeAnim, {
      toValue: 1,
      duration: 600,
      useNativeDriver: true,
    }).start();
  }, []);

  useEffect(() => {
    if (searchQuery.length > 0) {
      performSearch();
    } else {
      setSearchResults([]);
      setShowSuggestions(false);
    }
  }, [searchQuery, activeFilters, sortBy]);

  const performSearch = () => {
    setIsSearching(true);
    
    // Simulate search delay
    setTimeout(() => {
      let filtered = mockSearchResults.filter(result => {
        const matchesQuery = 
          result.title.toLowerCase().includes(searchQuery.toLowerCase()) ||
          result.tags.some(tag => tag.toLowerCase().includes(searchQuery.toLowerCase())) ||
          result.filename.toLowerCase().includes(searchQuery.toLowerCase()) ||
          (result.camera && result.camera.toLowerCase().includes(searchQuery.toLowerCase())) ||
          (result.location && result.location.toLowerCase().includes(searchQuery.toLowerCase()));

        const matchesFilters = activeFilters.every(filter => {
          switch (filter.type) {
            case 'tag':
              return result.tags.some(tag => tag.toLowerCase().includes(filter.value.toLowerCase()));
            case 'preset':
              return result.presetApplied?.toLowerCase().includes(filter.value.toLowerCase());
            case 'location':
              return result.location?.toLowerCase().includes(filter.value.toLowerCase());
            case 'camera':
              return result.camera?.toLowerCase().includes(filter.value.toLowerCase());
            case 'date':
              // Simple date filtering - could be more sophisticated
              return result.timestamp.includes(filter.value);
            default:
              return true;
          }
        });

        return matchesQuery && matchesFilters;
      });

      // Sort results
      filtered = sortResults(filtered);
      
      setSearchResults(filtered);
      setIsSearching(false);
      setShowSuggestions(false);
    }, 500);
  };

  const sortResults = (results: SearchResult[]): SearchResult[] => {
    switch (sortBy) {
      case 'time':
        return results.sort((a, b) => new Date(b.timestamp).getTime() - new Date(a.timestamp).getTime());
      case 'camera':
        return results.sort((a, b) => (a.camera || '').localeCompare(b.camera || ''));
      case 'preset':
        return results.sort((a, b) => (a.presetApplied || '').localeCompare(b.presetApplied || ''));
      default:
        return results;
    }
  };

  const addFilter = (type: SearchFilter['type'], value: string, label: string) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    
    const newFilter: SearchFilter = { type, value, label };
    setActiveFilters(prev => [...prev, newFilter]);
    setSearchQuery('');
    searchInputRef.current?.blur();
  };

  const removeFilter = (filterIndex: number) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    setActiveFilters(prev => prev.filter((_, index) => index !== filterIndex));
  };

  const clearAllFilters = () => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Medium);
    setActiveFilters([]);
  };

  const handleSortChange = (newSort: typeof sortBy) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    setSortBy(newSort);
  };

  const getSearchSuggestions = () => {
    if (searchQuery.length < 2) return [];
    
    const suggestions: string[] = [];
    
    // Tag suggestions
    mockSearchResults.forEach(result => {
      result.tags.forEach(tag => {
        if (tag.toLowerCase().includes(searchQuery.toLowerCase()) && !suggestions.includes(tag)) {
          suggestions.push(tag);
        }
      });
    });
    
    // Camera suggestions
    mockSearchResults.forEach(result => {
      if (result.camera && result.camera.toLowerCase().includes(searchQuery.toLowerCase()) && !suggestions.includes(result.camera)) {
        suggestions.push(result.camera);
      }
    });
    
    // Location suggestions
    mockSearchResults.forEach(result => {
      if (result.location && result.location.toLowerCase().includes(searchQuery.toLowerCase()) && !suggestions.includes(result.location)) {
        suggestions.push(result.location);
      }
    });
    
    return suggestions.slice(0, 5);
  };

  const renderSearchResult = ({ item }: { item: SearchResult }) => (
    <TouchableOpacity style={styles.resultCard} activeOpacity={0.8}>
      <Image source={{ uri: item.thumbnail }} style={styles.resultThumbnail} />
      
      <View style={styles.resultContent}>
        <Text style={styles.resultTitle} numberOfLines={1}>
          {item.title}
        </Text>
        
        <View style={styles.resultMeta}>
          <Text style={styles.resultFilename}>{item.filename}</Text>
          <Text style={styles.resultSize}>{item.size}</Text>
        </View>
        
        <View style={styles.resultTags}>
          {item.tags.slice(0, 3).map((tag, index) => (
            <View key={index} style={styles.resultTag}>
              <Text style={styles.resultTagText}>#{tag}</Text>
            </View>
          ))}
          {item.tags.length > 3 && (
            <Text style={styles.moreTagsText}>+{item.tags.length - 3}</Text>
          )}
        </View>
        
        <View style={styles.resultDetails}>
          {item.camera && (
            <View style={styles.resultDetail}>
              <Ionicons name="camera-outline" size={14} color={theme.colors.text.tertiary} />
              <Text style={styles.resultDetailText}>{item.camera}</Text>
            </View>
          )}
          
          {item.presetApplied && (
            <View style={styles.resultDetail}>
              <Ionicons name="brush-outline" size={14} color={theme.colors.text.tertiary} />
              <Text style={styles.resultDetailText}>{item.presetApplied}</Text>
            </View>
          )}
        </View>
      </View>
      
      <TouchableOpacity style={styles.resultAction}>
        <Ionicons name="ellipsis-vertical" size={20} color={theme.colors.text.tertiary} />
      </TouchableOpacity>
    </TouchableOpacity>
  );

  const renderSearchSuggestion = (suggestion: string) => (
    <TouchableOpacity
      key={suggestion}
      style={styles.suggestionItem}
      onPress={() => addFilter('tag', suggestion, suggestion)}
    >
      <Ionicons name="search" size={16} color={theme.colors.text.secondary} />
      <Text style={styles.suggestionText}>{suggestion}</Text>
      <Ionicons name="add-circle-outline" size={20} color={theme.colors.text.tertiary} />
    </TouchableOpacity>
  );

  return (
    <SafeAreaView style={styles.container}>
      {/* Header */}
      <View style={styles.header}>
        <Text style={styles.headerTitle}>Search</Text>
        <TouchableOpacity style={styles.headerButton}>
          <Ionicons name="options-outline" size={24} color={theme.colors.text.secondary} />
        </TouchableOpacity>
      </View>

      {/* Search Bar */}
      <View style={styles.searchContainer}>
        <View style={styles.searchBar}>
          <Ionicons name="search" size={20} color={theme.colors.text.secondary} />
          <TextInput
            ref={searchInputRef}
            style={styles.searchInput}
            placeholder="Search images, tags, cameras..."
            placeholderTextColor={theme.colors.text.tertiary}
            value={searchQuery}
            onChangeText={setSearchQuery}
            onFocus={() => setShowSuggestions(true)}
            onBlur={() => setTimeout(() => setShowSuggestions(false), 200)}
          />
          {searchQuery.length > 0 && (
            <TouchableOpacity onPress={() => setSearchQuery('')}>
              <Ionicons name="close-circle" size={20} color={theme.colors.text.tertiary} />
            </TouchableOpacity>
          )}
        </View>
      </View>

      {/* Active Filters */}
      {activeFilters.length > 0 && (
        <View style={styles.filtersContainer}>
          <ScrollView 
            horizontal 
            showsHorizontalScrollIndicator={false}
            contentContainerStyle={styles.filtersContent}
          >
            {activeFilters.map((filter, index) => (
              <View key={index} style={styles.filterChip}>
                <Text style={styles.filterChipText}>{filter.label}</Text>
                <TouchableOpacity onPress={() => removeFilter(index)}>
                  <Ionicons name="close" size={16} color={theme.colors.text.secondary} />
                </TouchableOpacity>
              </View>
            ))}
            <TouchableOpacity style={styles.clearFiltersButton} onPress={clearAllFilters}>
              <Text style={styles.clearFiltersText}>Clear All</Text>
            </TouchableOpacity>
          </ScrollView>
        </View>
      )}

      {/* Sort Options */}
      <View style={styles.sortContainer}>
        <Text style={styles.sortLabel}>Sort by:</Text>
        <ScrollView 
          horizontal 
          showsHorizontalScrollIndicator={false}
          contentContainerStyle={styles.sortOptions}
        >
          {[
            { key: 'relevance', label: 'Relevance' },
            { key: 'time', label: 'Time' },
            { key: 'camera', label: 'Camera' },
            { key: 'preset', label: 'Preset' },
          ].map((option) => (
            <TouchableOpacity
              key={option.key}
              style={[
                styles.sortOption,
                sortBy === option.key && styles.sortOptionActive
              ]}
              onPress={() => handleSortChange(option.key as typeof sortBy)}
            >
              <Text style={[
                styles.sortOptionText,
                sortBy === option.key && styles.sortOptionTextActive
              ]}>
                {option.label}
              </Text>
            </TouchableOpacity>
          ))}
        </ScrollView>
      </View>

      {/* Search Suggestions */}
      {showSuggestions && searchQuery.length > 0 && (
        <View style={styles.suggestionsContainer}>
          {getSearchSuggestions().map(renderSearchSuggestion)}
        </View>
      )}

      {/* Search Results */}
      <Animated.View style={[styles.resultsContainer, { opacity: fadeAnim }]}>
        {isSearching ? (
          <View style={styles.loadingContainer}>
            <Ionicons name="search" size={48} color={theme.colors.text.tertiary} />
            <Text style={styles.loadingText}>Searching...</Text>
          </View>
        ) : searchResults.length > 0 ? (
          <FlatList
            data={searchResults}
            renderItem={renderSearchResult}
            keyExtractor={(item) => item.id}
            showsVerticalScrollIndicator={false}
            contentContainerStyle={styles.resultsList}
          />
        ) : searchQuery.length > 0 ? (
          <View style={styles.emptyContainer}>
            <Ionicons name="search-outline" size={64} color={theme.colors.text.tertiary} />
            <Text style={styles.emptyTitle}>No results found</Text>
            <Text style={styles.emptySubtitle}>
              Try adjusting your search terms or filters
            </Text>
          </View>
        ) : (
          <View style={styles.initialContainer}>
            <Ionicons name="search-outline" size={64} color={theme.colors.text.tertiary} />
            <Text style={styles.initialTitle}>Search your photos</Text>
            <Text style={styles.initialSubtitle}>
              Find images by tags, camera, location, or filename
            </Text>
          </View>
        )}
      </Animated.View>
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
    marginRight: theme.spacing.s,
    fontSize: theme.fontSizes.body,
    color: theme.colors.text.primary,
    fontWeight: '400',
  },
  filtersContainer: {
    marginBottom: theme.spacing.m,
  },
  filtersContent: {
    paddingHorizontal: theme.spacing.l,
  },
  filterChip: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: theme.colors.green,
    paddingHorizontal: theme.spacing.m,
    paddingVertical: theme.spacing.s,
    borderRadius: theme.borderRadius.m,
    marginRight: theme.spacing.s,
    gap: theme.spacing.xs,
  },
  filterChipText: {
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.surface,
  },
  clearFiltersButton: {
    paddingHorizontal: theme.spacing.m,
    paddingVertical: theme.spacing.s,
    backgroundColor: theme.colors.lightGray,
    borderRadius: theme.borderRadius.m,
  },
  clearFiltersText: {
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.text.secondary,
  },
  sortContainer: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: theme.spacing.l,
    marginBottom: theme.spacing.l,
  },
  sortLabel: {
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.text.secondary,
    marginRight: theme.spacing.m,
  },
  sortOptions: {
    gap: theme.spacing.s,
  },
  sortOption: {
    paddingHorizontal: theme.spacing.m,
    paddingVertical: theme.spacing.s,
    backgroundColor: theme.colors.lightGray,
    borderRadius: theme.borderRadius.m,
  },
  sortOptionActive: {
    backgroundColor: theme.colors.green,
  },
  sortOptionText: {
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.text.secondary,
  },
  sortOptionTextActive: {
    color: theme.colors.surface,
  },
  suggestionsContainer: {
    backgroundColor: theme.colors.surface,
    marginHorizontal: theme.spacing.l,
    marginBottom: theme.spacing.m,
    borderRadius: theme.borderRadius.l,
    ...theme.shadows.subtle,
  },
  suggestionItem: {
    flexDirection: 'row',
    alignItems: 'center',
    padding: theme.spacing.m,
    borderBottomWidth: 1,
    borderBottomColor: theme.colors.border,
    gap: theme.spacing.m,
  },
  suggestionText: {
    flex: 1,
    fontSize: theme.fontSizes.body,
    color: theme.colors.text.primary,
    fontWeight: '500',
  },
  resultsContainer: {
    flex: 1,
  },
  loadingContainer: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    gap: theme.spacing.m,
  },
  loadingText: {
    fontSize: theme.fontSizes.body,
    color: theme.colors.text.secondary,
    fontWeight: '500',
  },
  emptyContainer: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    paddingHorizontal: theme.spacing.xl,
    gap: theme.spacing.m,
  },
  emptyTitle: {
    fontSize: theme.fontSizes.title,
    fontWeight: '700',
    color: theme.colors.text.primary,
    textAlign: 'center',
  },
  emptySubtitle: {
    fontSize: theme.fontSizes.body,
    color: theme.colors.text.secondary,
    textAlign: 'center',
    lineHeight: 20,
  },
  initialContainer: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    paddingHorizontal: theme.spacing.xl,
    gap: theme.spacing.m,
  },
  initialTitle: {
    fontSize: theme.fontSizes.title,
    fontWeight: '700',
    color: theme.colors.text.primary,
    textAlign: 'center',
  },
  initialSubtitle: {
    fontSize: theme.fontSizes.body,
    color: theme.colors.text.secondary,
    textAlign: 'center',
    lineHeight: 20,
  },
  resultsList: {
    paddingHorizontal: theme.spacing.l,
    paddingBottom: theme.spacing.xl,
  },
  resultCard: {
    flexDirection: 'row',
    backgroundColor: theme.colors.surface,
    borderRadius: theme.borderRadius.l,
    padding: theme.spacing.m,
    marginBottom: theme.spacing.s,
    ...theme.shadows.subtle,
    borderWidth: 1,
    borderColor: theme.colors.border,
  },
  resultThumbnail: {
    width: 80,
    height: 80,
    borderRadius: theme.borderRadius.m,
    marginRight: theme.spacing.m,
  },
  resultContent: {
    flex: 1,
  },
  resultTitle: {
    fontSize: theme.fontSizes.headline,
    fontWeight: '700',
    color: theme.colors.text.primary,
    marginBottom: theme.spacing.xs,
  },
  resultMeta: {
    flexDirection: 'row',
    alignItems: 'center',
    marginBottom: theme.spacing.xs,
    gap: theme.spacing.m,
  },
  resultFilename: {
    fontSize: theme.fontSizes.caption,
    color: theme.colors.text.secondary,
    fontWeight: '500',
  },
  resultSize: {
    fontSize: theme.fontSizes.caption,
    color: theme.colors.text.tertiary,
    fontWeight: '500',
  },
  resultTags: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: theme.spacing.xs,
    marginBottom: theme.spacing.xs,
  },
  resultTag: {
    backgroundColor: theme.colors.lightGray,
    paddingHorizontal: theme.spacing.xs,
    paddingVertical: 2,
    borderRadius: theme.borderRadius.s,
  },
  resultTagText: {
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
  resultDetails: {
    flexDirection: 'row',
    gap: theme.spacing.m,
  },
  resultDetail: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: theme.spacing.xs,
  },
  resultDetailText: {
    fontSize: theme.fontSizes.caption,
    color: theme.colors.text.tertiary,
    fontWeight: '500',
  },
  resultAction: {
    padding: theme.spacing.s,
    alignSelf: 'flex-start',
  },
});

export default SearchScreen;
