import React, { useState } from 'react';
import { 
  View, 
  Text, 
  TouchableOpacity, 
  StyleSheet, 
  ScrollView 
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import * as Haptics from 'expo-haptics';
import { theme } from '../constants/theme';

interface FilterOption {
  id: string;
  label: string;
  icon: string;
  count?: number;
}

interface FilterSystemProps {
  onFilterChange: (filters: string[]) => void;
  activeFilters: string[];
}

const FilterSystem: React.FC<FilterSystemProps> = ({
  onFilterChange,
  activeFilters
}) => {
  const [isExpanded, setIsExpanded] = useState(false);

  const filterOptions: FilterOption[] = [
    { id: 'synced', label: 'Synced', icon: 'checkmark-circle', count: 89 },
    { id: 'unsynced', label: 'Unsynced', icon: 'sync', count: 12 },
    { id: 'published', label: 'Published', icon: 'globe', count: 36 },
    { id: 'edited', label: 'Edited', icon: 'create', count: 45 },
    { id: 'unedited', label: 'Unedited', icon: 'ellipse', count: 61 },
    { id: 'preset-applied', label: 'Preset Applied', icon: 'color-palette', count: 78 },
    { id: 'sony-a7iv', label: 'Sony A7 IV', icon: 'camera', count: 67 },
    { id: 'canon-r5', label: 'Canon R5', icon: 'camera', count: 34 },
  ];

  const handleFilterToggle = (filterId: string) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    
    const newFilters = activeFilters.includes(filterId)
      ? activeFilters.filter(id => id !== filterId)
      : [...activeFilters, filterId];
    
    onFilterChange(newFilters);
  };

  const handleExpandToggle = () => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    setIsExpanded(!isExpanded);
  };

  const renderFilterChip = (option: FilterOption) => {
    const isActive = activeFilters.includes(option.id);
    
    return (
      <TouchableOpacity
        key={option.id}
        style={[
          styles.filterChip,
          isActive && styles.filterChipActive
        ]}
        onPress={() => handleFilterToggle(option.id)}
        activeOpacity={0.8}
      >
        <Ionicons 
          name={option.icon as any} 
          size={14} 
          color={isActive ? theme.colors.surface : theme.colors.text.secondary} 
        />
        <Text style={[
          styles.filterChipText,
          isActive && styles.filterChipTextActive
        ]}>
          {option.label}
        </Text>
        {option.count !== undefined && (
          <View style={[
            styles.countBadge,
            isActive && styles.countBadgeActive
          ]}>
            <Text style={[
              styles.countText,
              isActive && styles.countTextActive
            ]}>
              {option.count}
            </Text>
          </View>
        )}
      </TouchableOpacity>
    );
  };

  return (
    <View style={styles.container}>
      {/* Header */}
      <TouchableOpacity 
        style={styles.header}
        onPress={handleExpandToggle}
        activeOpacity={0.8}
      >
        <View style={styles.headerLeft}>
          <Ionicons name="filter" size={18} color={theme.colors.semantic.secondary} />
          <Text style={styles.headerTitle}>Filters</Text>
          {activeFilters.length > 0 && (
            <View style={styles.activeFiltersBadge}>
              <Text style={styles.activeFiltersText}>{activeFilters.length}</Text>
            </View>
          )}
        </View>
        
        <Ionicons 
          name={isExpanded ? "chevron-up" : "chevron-down"} 
          size={20} 
          color={theme.colors.text.secondary} 
        />
      </TouchableOpacity>

      {/* Filter Options */}
      {isExpanded && (
        <View style={styles.filterOptions}>
          <ScrollView 
            horizontal 
            showsHorizontalScrollIndicator={false}
            contentContainerStyle={styles.filterScrollContent}
          >
            {filterOptions.map(renderFilterChip)}
          </ScrollView>
          
          {/* Clear Filters */}
          {activeFilters.length > 0 && (
            <TouchableOpacity
              style={styles.clearFiltersButton}
              onPress={() => onFilterChange([])}
            >
              <Ionicons name="close-circle" size={16} color={theme.colors.semantic.error} />
              <Text style={styles.clearFiltersText}>Clear Filters</Text>
            </TouchableOpacity>
          )}
        </View>
      )}
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    backgroundColor: theme.colors.surface,
    borderRadius: theme.borderRadius.m,
    marginHorizontal: theme.spacing.l,
    marginTop: theme.spacing.s,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 4,
    elevation: 3,
    overflow: 'hidden',
  },
  header: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    paddingHorizontal: theme.spacing.m,
    paddingVertical: theme.spacing.m,
    backgroundColor: 'rgba(88, 86, 214, 0.1)',
    borderBottomWidth: 1,
    borderBottomColor: 'rgba(88, 86, 214, 0.2)',
  },
  headerLeft: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: theme.spacing.s,
  },
  headerTitle: {
    fontSize: 16,
    fontWeight: '600',
    color: theme.colors.semantic.secondary,
  },
  activeFiltersBadge: {
    backgroundColor: theme.colors.semantic.secondary,
    paddingHorizontal: 8,
    paddingVertical: 2,
    borderRadius: 10,
  },
  activeFiltersText: {
    fontSize: 11,
    fontWeight: '700',
    color: theme.colors.surface,
  },
  filterOptions: {
    padding: theme.spacing.m,
  },
  filterScrollContent: {
    gap: 8,
    paddingBottom: theme.spacing.s,
  },
  filterChip: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: 'rgba(0, 0, 0, 0.05)',
    paddingHorizontal: 12,
    paddingVertical: 8,
    borderRadius: 20,
    borderWidth: 1,
    borderColor: 'rgba(0, 0, 0, 0.1)',
    gap: 6,
  },
  filterChipActive: {
    backgroundColor: theme.colors.semantic.secondary,
    borderColor: theme.colors.semantic.secondary,
  },
  filterChipText: {
    fontSize: 13,
    fontWeight: '500',
    color: theme.colors.text.primary,
  },
  filterChipTextActive: {
    color: theme.colors.surface,
  },
  countBadge: {
    backgroundColor: 'rgba(0, 0, 0, 0.1)',
    paddingHorizontal: 6,
    paddingVertical: 2,
    borderRadius: 8,
  },
  countBadgeActive: {
    backgroundColor: 'rgba(255, 255, 255, 0.2)',
  },
  countText: {
    fontSize: 10,
    fontWeight: '600',
    color: theme.colors.text.secondary,
  },
  countTextActive: {
    color: theme.colors.surface,
  },
  clearFiltersButton: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'center',
    paddingVertical: theme.spacing.s,
    gap: 6,
    marginTop: theme.spacing.s,
  },
  clearFiltersText: {
    fontSize: 14,
    fontWeight: '500',
    color: theme.colors.semantic.error,
  },
});

export default FilterSystem;
