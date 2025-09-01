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

interface AutoTagItem {
  id: string;
  originalName: string;
  suggestedName: string;
  confidence: number;
  tags: string[];
  suggestedTags: string[];
  isSelected: boolean;
  status: 'pending' | 'processing' | 'completed' | 'failed';
}

interface AutoTagManagerProps {
  isVisible: boolean;
  onClose: () => void;
  onApply: (items: AutoTagItem[]) => void;
}

const AutoTagManager: React.FC<AutoTagManagerProps> = ({
  isVisible,
  onClose,
  onApply,
}) => {
  const [autoTagItems, setAutoTagItems] = useState<AutoTagItem[]>([]);
  const [isProcessing, setIsProcessing] = useState(false);
  const [selectedItems, setSelectedItems] = useState<string[]>([]);
  const [autoRenameEnabled, setAutoRenameEnabled] = useState(true);
  const [autoTagEnabled, setAutoTagEnabled] = useState(true);
  const [mergeDuplicatesEnabled, setMergeDuplicatesEnabled] = useState(true);
  const [searchQuery, setSearchQuery] = useState('');
  const [showAdvanced, setShowAdvanced] = useState(false);
  
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
      
      loadMockData();
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

  const loadMockData = () => {
    const mockItems: AutoTagItem[] = [
      {
        id: '1',
        originalName: 'IMG_20240115_103045.jpg',
        suggestedName: 'Mountain_Landscape_2024.jpg',
        confidence: 0.95,
        tags: ['IMG', '2024', '01', '15'],
        suggestedTags: ['landscape', 'mountains', 'nature', 'vivid', 'golden-hour'],
        isSelected: false,
        status: 'pending',
      },
      {
        id: '2',
        originalName: 'DSC_0001.NEF',
        suggestedName: 'Forest_Path_Canon.jpg',
        confidence: 0.87,
        tags: ['DSC', '0001', 'NEF'],
        suggestedTags: ['forest', 'path', 'green', 'moody', 'canon'],
        isSelected: false,
        status: 'pending',
      },
      {
        id: '3',
        originalName: 'P1000123.JPG',
        suggestedName: 'City_Skyline_Night.jpg',
        confidence: 0.92,
        tags: ['P100', '0123', 'JPG'],
        suggestedTags: ['city', 'skyline', 'urban', 'night', 'dramatic'],
        isSelected: false,
        status: 'pending',
      },
      {
        id: '4',
        originalName: 'IMG_20240115_074520.jpg',
        suggestedName: 'Ocean_Waves_Sunrise.jpg',
        confidence: 0.89,
        tags: ['IMG', '2024', '01', '15', '07'],
        suggestedTags: ['ocean', 'waves', 'sunrise', 'blue', 'peaceful'],
        isSelected: false,
        status: 'pending',
      },
    ];
    
    setAutoTagItems(mockItems);
  };

  const handleItemToggle = (itemId: string) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    
    setSelectedItems(prev => {
      if (prev.includes(itemId)) {
        return prev.filter(id => id !== itemId);
      } else {
        return [...prev, itemId];
      }
    });
    
    setAutoTagItems(prev => 
      prev.map(item => 
        item.id === itemId 
          ? { ...item, isSelected: !item.isSelected }
          : item
      )
    );
  };

  const handleSelectAll = () => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Medium);
    const allIds = autoTagItems.map(item => item.id);
    setSelectedItems(allIds);
    setAutoTagItems(prev => 
      prev.map(item => ({ ...item, isSelected: true }))
    );
  };

  const handleClearSelection = () => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    setSelectedItems([]);
    setAutoTagItems(prev => 
      prev.map(item => ({ ...item, isSelected: false }))
    );
  };

  const handleProcessSelected = async () => {
    if (selectedItems.length === 0) {
      Alert.alert('No Items Selected', 'Please select at least one item to process.');
      return;
    }

    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Medium);
    setIsProcessing(true);

    // Simulate AI processing
    const processingSteps = [
      'Analyzing image content...',
      'Generating smart tags...',
      'Cleaning filenames...',
      'Merging duplicate tags...',
      'Finalizing suggestions...',
    ];

    for (let i = 0; i < processingSteps.length; i++) {
      await new Promise(resolve => setTimeout(resolve, 800));
      
      setAutoTagItems(prev => 
        prev.map(item => 
          selectedItems.includes(item.id)
            ? { ...item, status: 'processing' as const }
            : item
        )
      );
    }

    // Complete processing
    setAutoTagItems(prev => 
      prev.map(item => 
        selectedItems.includes(item.id)
          ? { ...item, status: 'completed' as const }
          : item
      )
    );

    setIsProcessing(false);
    Alert.alert(
      'Processing Complete', 
      `Successfully processed ${selectedItems.length} items with AI-powered suggestions.`
    );
  };

  const handleApplyChanges = () => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Medium);
    
    const itemsToApply = autoTagItems.filter(item => 
      selectedItems.includes(item.id) && item.status === 'completed'
    );
    
    if (itemsToApply.length === 0) {
      Alert.alert('No Changes to Apply', 'Please process some items first before applying changes.');
      return;
    }
    
    onApply(itemsToApply);
    onClose();
  };

  const filteredItems = autoTagItems.filter(item => {
    const matchesSearch = 
      item.originalName.toLowerCase().includes(searchQuery.toLowerCase()) ||
      item.suggestedName.toLowerCase().includes(searchQuery.toLowerCase()) ||
      item.suggestedTags.some(tag => tag.toLowerCase().includes(searchQuery.toLowerCase()));
    
    return matchesSearch;
  });

  const getStatusIcon = (status: AutoTagItem['status']) => {
    switch (status) {
      case 'pending': return 'time-outline';
      case 'processing': return 'sync-outline';
      case 'completed': return 'checkmark-circle-outline';
      case 'failed': return 'close-circle-outline';
      default: return 'ellipse-outline';
    }
  };

  const getStatusColor = (status: AutoTagItem['status']) => {
    switch (status) {
      case 'pending': return theme.colors.system.orange;
      case 'processing': return theme.colors.system.blue;
      case 'completed': return theme.colors.system.green;
      case 'failed': return theme.colors.system.red;
      default: return theme.colors.gray;
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
            <Text style={styles.title}>AI Auto Manager</Text>
            <View style={styles.aiBadge}>
              <Ionicons name="sparkles" size={12} color={theme.colors.surface} />
              <Text style={styles.aiBadgeText}>AI Powered</Text>
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
              placeholder="Search files or tags..."
              placeholderTextColor={theme.colors.text.tertiary}
              value={searchQuery}
              onChangeText={setSearchQuery}
            />
          </View>
        </View>

        {/* Settings Toggle */}
        <TouchableOpacity 
          style={styles.settingsToggle}
          onPress={() => setShowAdvanced(!showAdvanced)}
        >
          <Text style={styles.settingsToggleText}>Advanced Settings</Text>
          <Ionicons 
            name={showAdvanced ? "chevron-up" : "chevron-down"} 
            size={20} 
            color={theme.colors.text.secondary} 
          />
        </TouchableOpacity>

        {/* Advanced Settings */}
        {showAdvanced && (
          <View style={styles.advancedSettings}>
            <View style={styles.settingRow}>
              <Text style={styles.settingLabel}>Auto-rename files</Text>
              <Switch
                value={autoRenameEnabled}
                onValueChange={setAutoRenameEnabled}
                trackColor={{ false: theme.colors.lightGray, true: theme.colors.green }}
                thumbColor={theme.colors.surface}
              />
            </View>
            <View style={styles.settingRow}>
              <Text style={styles.settingLabel}>Auto-generate tags</Text>
              <Switch
                value={autoTagEnabled}
                onValueChange={setAutoTagEnabled}
                trackColor={{ false: theme.colors.lightGray, true: theme.colors.green }}
                thumbColor={theme.colors.surface}
              />
            </View>
            <View style={styles.settingRow}>
              <Text style={styles.settingLabel}>Merge duplicate tags</Text>
              <Switch
                value={mergeDuplicatesEnabled}
                onValueChange={setMergeDuplicatesEnabled}
                trackColor={{ false: theme.colors.lightGray, true: theme.colors.green }}
                thumbColor={theme.colors.surface}
              />
            </View>
          </View>
        )}

        {/* Selection Controls */}
        <View style={styles.selectionControls}>
          <View style={styles.selectionInfo}>
            <Text style={styles.selectionText}>
              {selectedItems.length} of {autoTagItems.length} selected
            </Text>
          </View>
          <View style={styles.selectionButtons}>
            <TouchableOpacity style={styles.selectionButton} onPress={handleSelectAll}>
              <Text style={styles.selectionButtonText}>Select All</Text>
            </TouchableOpacity>
            <TouchableOpacity style={styles.selectionButton} onPress={handleClearSelection}>
              <Text style={styles.selectionButtonText}>Clear</Text>
            </TouchableOpacity>
          </View>
        </View>

        {/* Items List */}
        <ScrollView style={styles.itemsContainer} showsVerticalScrollIndicator={false}>
          {filteredItems.map((item) => (
            <View key={item.id} style={styles.itemCard}>
              {/* Selection Checkbox */}
              <TouchableOpacity 
                style={styles.checkbox}
                onPress={() => handleItemToggle(item.id)}
              >
                <Ionicons 
                  name={item.isSelected ? "checkbox" : "square-outline"} 
                  size={24} 
                  color={item.isSelected ? theme.colors.green : theme.colors.text.tertiary} 
                />
              </TouchableOpacity>

              {/* Item Content */}
              <View style={styles.itemContent}>
                {/* Filename Section */}
                <View style={styles.filenameSection}>
                  <View style={styles.originalName}>
                    <Text style={styles.originalNameLabel}>Original:</Text>
                    <Text style={styles.originalNameText} numberOfLines={1}>
                      {item.originalName}
                    </Text>
                  </View>
                  
                  {autoRenameEnabled && (
                    <View style={styles.suggestedName}>
                      <Text style={styles.suggestedNameLabel}>Suggested:</Text>
                      <Text style={styles.suggestedNameText} numberOfLines={1}>
                        {item.suggestedName}
                      </Text>
                      <View style={styles.confidenceBadge}>
                        <Text style={styles.confidenceText}>
                          {Math.round(item.confidence * 100)}%
                        </Text>
                      </View>
                    </View>
                  )}
                </View>

                {/* Tags Section */}
                {autoTagEnabled && (
                  <View style={styles.tagsSection}>
                    <Text style={styles.tagsLabel}>Current Tags:</Text>
                    <View style={styles.currentTags}>
                      {item.tags.map((tag, index) => (
                        <View key={index} style={styles.currentTag}>
                          <Text style={styles.currentTagText}>#{tag}</Text>
                        </View>
                      ))}
                    </View>
                    
                    <Text style={styles.tagsLabel}>AI Suggested Tags:</Text>
                    <View style={styles.suggestedTags}>
                      {item.suggestedTags.map((tag, index) => (
                        <View key={index} style={styles.suggestedTag}>
                          <Ionicons name="sparkles" size={12} color={theme.colors.system.yellow} />
                          <Text style={styles.suggestedTagText}>#{tag}</Text>
                        </View>
                      ))}
                    </View>
                  </View>
                )}

                {/* Status */}
                <View style={styles.itemStatus}>
                  <View style={[styles.statusIndicator, { backgroundColor: getStatusColor(item.status) }]} />
                  <Text style={styles.statusText}>
                    {item.status.charAt(0).toUpperCase() + item.status.slice(1)}
                  </Text>
                </View>
              </View>
            </View>
          ))}
        </ScrollView>

        {/* Action Buttons */}
        <View style={styles.actions}>
          <TouchableOpacity 
            style={styles.processButton}
            onPress={handleProcessSelected}
            disabled={isProcessing || selectedItems.length === 0}
          >
            <Ionicons 
              name={isProcessing ? "sync" : "sparkles"} 
              size={20} 
              color={theme.colors.surface} 
            />
            <Text style={styles.processButtonText}>
              {isProcessing ? 'Processing...' : 'Process Selected'}
            </Text>
          </TouchableOpacity>
          
          <TouchableOpacity 
            style={styles.applyButton}
            onPress={handleApplyChanges}
            disabled={selectedItems.length === 0}
          >
            <Ionicons name="checkmark" size={20} color={theme.colors.surface} />
            <Text style={styles.applyButtonText}>
              Apply Changes ({selectedItems.length})
            </Text>
          </TouchableOpacity>
        </View>
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
    width: 380,
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
  aiBadge: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: theme.colors.green,
    paddingHorizontal: theme.spacing.xs,
    paddingVertical: 2,
    borderRadius: theme.borderRadius.s,
    gap: 2,
  },
  aiBadgeText: {
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
  settingsToggle: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingHorizontal: theme.spacing.l,
    paddingVertical: theme.spacing.m,
    borderBottomWidth: 1,
    borderBottomColor: theme.colors.border,
  },
  settingsToggleText: {
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.text.primary,
  },
  advancedSettings: {
    paddingHorizontal: theme.spacing.l,
    paddingBottom: theme.spacing.m,
    borderBottomWidth: 1,
    borderBottomColor: theme.colors.border,
  },
  settingRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingVertical: theme.spacing.s,
  },
  settingLabel: {
    fontSize: theme.fontSizes.body,
    color: theme.colors.text.primary,
    fontWeight: '500',
  },
  selectionControls: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    padding: theme.spacing.l,
    borderBottomWidth: 1,
    borderBottomColor: theme.colors.border,
  },
  selectionInfo: {
    flex: 1,
  },
  selectionText: {
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.text.primary,
  },
  selectionButtons: {
    flexDirection: 'row',
    gap: theme.spacing.s,
  },
  selectionButton: {
    paddingHorizontal: theme.spacing.m,
    paddingVertical: theme.spacing.s,
    backgroundColor: theme.colors.lightGray,
    borderRadius: theme.borderRadius.m,
  },
  selectionButtonText: {
    fontSize: theme.fontSizes.caption,
    fontWeight: '600',
    color: theme.colors.text.secondary,
  },
  itemsContainer: {
    flex: 1,
    paddingHorizontal: theme.spacing.l,
  },
  itemCard: {
    flexDirection: 'row',
    backgroundColor: theme.colors.lightGray,
    borderRadius: theme.borderRadius.m,
    padding: theme.spacing.m,
    marginBottom: theme.spacing.s,
    borderWidth: 2,
    borderColor: 'transparent',
  },
  checkbox: {
    marginRight: theme.spacing.m,
    alignSelf: 'flex-start',
  },
  itemContent: {
    flex: 1,
  },
  filenameSection: {
    marginBottom: theme.spacing.m,
  },
  originalName: {
    marginBottom: theme.spacing.xs,
  },
  originalNameLabel: {
    fontSize: theme.fontSizes.caption,
    fontWeight: '600',
    color: theme.colors.text.secondary,
    marginBottom: 2,
  },
  originalNameText: {
    fontSize: theme.fontSizes.body,
    color: theme.colors.text.primary,
    fontWeight: '500',
  },
  suggestedName: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: theme.spacing.s,
  },
  suggestedNameLabel: {
    fontSize: theme.fontSizes.caption,
    fontWeight: '600',
    color: theme.colors.text.secondary,
  },
  suggestedNameText: {
    fontSize: theme.fontSizes.body,
    color: theme.colors.green,
    fontWeight: '600',
  },
  confidenceBadge: {
    backgroundColor: theme.colors.green + '20',
    paddingHorizontal: theme.spacing.xs,
    paddingVertical: 2,
    borderRadius: theme.borderRadius.s,
  },
  confidenceText: {
    fontSize: theme.fontSizes.caption,
    fontWeight: '700',
    color: theme.colors.green,
  },
  tagsSection: {
    marginBottom: theme.spacing.m,
  },
  tagsLabel: {
    fontSize: theme.fontSizes.caption,
    fontWeight: '600',
    color: theme.colors.text.secondary,
    marginBottom: theme.spacing.xs,
  },
  currentTags: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: theme.spacing.xs,
    marginBottom: theme.spacing.s,
  },
  currentTag: {
    backgroundColor: theme.colors.lightGray,
    paddingHorizontal: theme.spacing.xs,
    paddingVertical: 2,
    borderRadius: theme.borderRadius.s,
  },
  currentTagText: {
    fontSize: theme.fontSizes.caption,
    color: theme.colors.text.secondary,
    fontWeight: '500',
  },
  suggestedTags: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: theme.spacing.xs,
  },
  suggestedTag: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: theme.colors.system.yellow + '20',
    paddingHorizontal: theme.spacing.xs,
    paddingVertical: 2,
    borderRadius: theme.borderRadius.s,
    gap: 2,
  },
  suggestedTagText: {
    fontSize: theme.fontSizes.caption,
    color: theme.colors.system.yellow,
    fontWeight: '600',
  },
  itemStatus: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: theme.spacing.xs,
  },
  statusIndicator: {
    width: 8,
    height: 8,
    borderRadius: 4,
  },
  statusText: {
    fontSize: theme.fontSizes.caption,
    fontWeight: '600',
    color: theme.colors.text.secondary,
  },
  actions: {
    padding: theme.spacing.l,
    gap: theme.spacing.m,
    borderTopWidth: 1,
    borderTopColor: theme.colors.border,
  },
  processButton: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'center',
    paddingVertical: theme.spacing.m,
    paddingHorizontal: theme.spacing.l,
    backgroundColor: theme.colors.system.blue,
    borderRadius: theme.borderRadius.m,
    gap: theme.spacing.s,
  },
  processButtonText: {
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.surface,
  },
  applyButton: {
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

export default AutoTagManager;
