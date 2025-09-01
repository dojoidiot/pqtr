import React, { useState, useEffect, useRef } from 'react';
import {
  View,
  Text,
  TouchableOpacity,
  StyleSheet,
  ScrollView,
  Animated,
  Image,
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import * as Haptics from 'expo-haptics';
import { theme } from '../constants/theme';

interface EditHistoryItem {
  id: string;
  timestamp: string;
  action: 'preset_applied' | 'manual_adjustment' | 'crop' | 'filter' | 'export' | 'revert';
  description: string;
  presetName?: string;
  adjustments?: {
    brightness?: number;
    contrast?: number;
    saturation?: number;
    temperature?: number;
    tint?: number;
    highlights?: number;
    shadows?: number;
    sharpness?: number;
    vignette?: number;
  };
  beforeImage?: string;
  afterImage?: string;
  isRevertable: boolean;
  isSelected: boolean;
}

interface EditHistoryPanelProps {
  isVisible: boolean;
  onClose: () => void;
  onRevert: (historyItem: EditHistoryItem) => void;
  onCompare: (beforeImage: string, afterImage: string) => void;
}

const EditHistoryPanel: React.FC<EditHistoryPanelProps> = ({
  isVisible,
  onClose,
  onRevert,
  onCompare,
}) => {
  const [editHistory, setEditHistory] = useState<EditHistoryItem[]>([]);
  const [selectedItem, setSelectedItem] = useState<EditHistoryItem | null>(null);
  const [showBeforeAfter, setShowBeforeAfter] = useState(false);
  const [compareMode, setCompareMode] = useState(false);
  const [compareImages, setCompareImages] = useState<{ before: string; after: string } | null>(null);
  
  const slideAnim = useRef(new Animated.Value(300)).current;
  const fadeAnim = useRef(new Animated.Value(0)).current;
  const compareAnim = useRef(new Animated.Value(0)).current;

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
      
      loadEditHistory();
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

  const loadEditHistory = () => {
    const mockHistory: EditHistoryItem[] = [
      {
        id: '1',
        timestamp: '2024-01-15T10:30:00Z',
        action: 'preset_applied',
        description: 'Applied Vivid Landscape preset',
        presetName: 'Vivid Landscape',
        adjustments: {
          brightness: 15,
          contrast: 25,
          saturation: 30,
          temperature: 5,
          tint: -2,
          highlights: -10,
          shadows: 15,
          sharpness: 20,
          vignette: 15,
        },
        beforeImage: 'https://images.unsplash.com/photo-1506905925346-21bda4d32df4?w=200',
        afterImage: 'https://images.unsplash.com/photo-1506905925346-21bda4d32df4?w=200',
        isRevertable: true,
        isSelected: false,
      },
      {
        id: '2',
        timestamp: '2024-01-15T10:25:00Z',
        action: 'manual_adjustment',
        description: 'Fine-tuned exposure and shadows',
        adjustments: {
          brightness: 5,
          shadows: 20,
          highlights: -5,
        },
        beforeImage: 'https://images.unsplash.com/photo-1506905925346-21bda4d32df4?w=200',
        afterImage: 'https://images.unsplash.com/photo-1506905925346-21bda4d32df4?w=200',
        isRevertable: true,
        isSelected: false,
      },
      {
        id: '3',
        timestamp: '2024-01-15T10:20:00Z',
        action: 'crop',
        description: 'Cropped to 16:9 aspect ratio',
        beforeImage: 'https://images.unsplash.com/photo-1506905925346-21bda4d32df4?w=200',
        afterImage: 'https://images.unsplash.com/photo-1506905925346-21bda4d32df4?w=200',
        isRevertable: true,
        isSelected: false,
      },
      {
        id: '4',
        timestamp: '2024-01-15T10:15:00Z',
        action: 'filter',
        description: 'Applied subtle vignette effect',
        adjustments: {
          vignette: 25,
        },
        beforeImage: 'https://images.unsplash.com/photo-1506905925346-21bda4d32df4?w=200',
        afterImage: 'https://images.unsplash.com/photo-1506905925346-21bda4d32df4?w=200',
        isRevertable: true,
        isSelected: false,
      },
      {
        id: '5',
        timestamp: '2024-01-15T10:10:00Z',
        action: 'export',
        description: 'Exported for web (JPEG, 2K)',
        isRevertable: false,
        isSelected: false,
      },
    ];
    
    setEditHistory(mockHistory);
  };

  const handleItemPress = (item: EditHistoryItem) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    setSelectedItem(item);
    setShowBeforeAfter(true);
  };

  const handleRevert = (item: EditHistoryItem) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Medium);
    onRevert(item);
    
    // Update history to show revert action
    const revertItem: EditHistoryItem = {
      id: `revert_${Date.now()}`,
      timestamp: new Date().toISOString(),
      action: 'revert',
      description: `Reverted to: ${item.description}`,
      isRevertable: false,
      isSelected: false,
    };
    
    setEditHistory(prev => [revertItem, ...prev]);
    setSelectedItem(null);
    setShowBeforeAfter(false);
  };

  const handleCompare = (beforeImage: string, afterImage: string) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Medium);
    setCompareImages({ before: beforeImage, after: afterImage });
    setCompareMode(true);
    
    Animated.spring(compareAnim, {
      toValue: 1,
      useNativeDriver: true,
      tension: 100,
      friction: 8,
    }).start();
  };

  const handleCloseCompare = () => {
    setCompareMode(false);
    setCompareImages(null);
    
    Animated.spring(compareAnim, {
      toValue: 0,
      useNativeDriver: true,
      tension: 100,
      friction: 8,
    }).start();
  };

  const getActionIcon = (action: EditHistoryItem['action']) => {
    switch (action) {
      case 'preset_applied': return 'brush-outline';
      case 'manual_adjustment': return 'sliders-outline';
      case 'crop': return 'crop-outline';
      case 'filter': return 'color-filter-outline';
      case 'export': return 'download-outline';
      case 'revert': return 'arrow-undo-outline';
      default: return 'ellipse-outline';
    }
  };

  const getActionColor = (action: EditHistoryItem['action']) => {
    switch (action) {
      case 'preset_applied': return theme.colors.system.blue;
      case 'manual_adjustment': return theme.colors.system.green;
      case 'crop': return theme.colors.system.orange;
      case 'filter': return theme.colors.system.purple;
      case 'export': return theme.colors.system.teal;
      case 'revert': return theme.colors.system.red;
      default: return theme.colors.gray;
    }
  };

  const formatTimestamp = (timestamp: string) => {
    const date = new Date(timestamp);
    const now = new Date();
    const diffInMinutes = Math.floor((now.getTime() - date.getTime()) / (1000 * 60));
    
    if (diffInMinutes < 1) return 'Just now';
    if (diffInMinutes < 60) return `${diffInMinutes}m ago`;
    if (diffInMinutes < 1440) return `${Math.floor(diffInMinutes / 60)}h ago`;
    return date.toLocaleDateString();
  };

  const renderAdjustments = (adjustments?: EditHistoryItem['adjustments']) => {
    if (!adjustments) return null;
    
    const adjustmentItems = Object.entries(adjustments).filter(([_, value]) => value !== undefined);
    
    return (
      <View style={styles.adjustmentsContainer}>
        {adjustmentItems.map(([key, value]) => (
          <View key={key} style={styles.adjustmentItem}>
            <Text style={styles.adjustmentLabel}>{key}</Text>
            <View style={styles.adjustmentBar}>
              <View 
                style={[
                  styles.adjustmentFill, 
                  { 
                    width: `${Math.abs(value)}%`,
                    backgroundColor: value > 0 ? theme.colors.system.green : theme.colors.system.red,
                    left: value < 0 ? 'auto' : '0',
                    right: value < 0 ? '0' : 'auto',
                  }
                ]} 
              />
            </View>
            <Text style={styles.adjustmentValue}>{value > 0 ? '+' : ''}{value}</Text>
          </View>
        ))}
      </View>
    );
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
            <Text style={styles.title}>Edit History</Text>
            <View style={styles.historyCount}>
              <Text style={styles.historyCountText}>
                {editHistory.length} edits
              </Text>
            </View>
          </View>
          <TouchableOpacity onPress={onClose} style={styles.closeButton}>
            <Ionicons name="close" size={24} color={theme.colors.text.secondary} />
          </TouchableOpacity>
        </View>

        {/* Before/After Toggle */}
        <View style={styles.toggleSection}>
          <TouchableOpacity 
            style={[
              styles.toggleButton,
              showBeforeAfter && styles.toggleButtonActive
            ]}
            onPress={() => setShowBeforeAfter(!showBeforeAfter)}
          >
            <Ionicons 
              name="eye-outline" 
              size={20} 
              color={showBeforeAfter ? theme.colors.surface : theme.colors.text.secondary} 
            />
            <Text style={[
              styles.toggleButtonText,
              showBeforeAfter && styles.toggleButtonTextActive
            ]}>
              Before/After
            </Text>
          </TouchableOpacity>
        </View>

        {/* Edit History Timeline */}
        <ScrollView style={styles.historyContainer} showsVerticalScrollIndicator={false}>
          {editHistory.map((item, index) => (
            <View key={item.id} style={styles.historyItem}>
              {/* Timeline Line */}
              <View style={styles.timelineLine}>
                <View style={[styles.timelineDot, { backgroundColor: getActionColor(item.action) }]} />
                {index < editHistory.length - 1 && <View style={styles.timelineConnector} />}
              </View>

              {/* History Content */}
              <TouchableOpacity 
                style={styles.historyContent}
                onPress={() => handleItemPress(item)}
                activeOpacity={0.8}
              >
                {/* Action Header */}
                <View style={styles.actionHeader}>
                  <View style={styles.actionLeft}>
                    <View style={[styles.actionIcon, { backgroundColor: getActionColor(item.action) + '20' }]}>
                      <Ionicons 
                        name={getActionIcon(item.action)} 
                        size={16} 
                        color={getActionColor(item.action)} 
                      />
                    </View>
                    <View style={styles.actionInfo}>
                      <Text style={styles.actionDescription}>{item.description}</Text>
                      <Text style={styles.actionTimestamp}>
                        {formatTimestamp(item.timestamp)}
                      </Text>
                    </View>
                  </View>
                  
                  {item.presetName && (
                    <View style={styles.presetBadge}>
                      <Text style={styles.presetText}>{item.presetName}</Text>
                    </View>
                  )}
                </View>

                {/* Before/After Images */}
                {showBeforeAfter && item.beforeImage && item.afterImage && (
                  <View style={styles.beforeAfterContainer}>
                    <View style={styles.imageContainer}>
                      <Text style={styles.imageLabel}>Before</Text>
                      <Image source={{ uri: item.beforeImage }} style={styles.historyImage} />
                    </View>
                    <View style={styles.imageContainer}>
                      <Text style={styles.imageLabel}>After</Text>
                      <Image source={{ uri: item.afterImage }} style={styles.historyImage} />
                    </View>
                    <TouchableOpacity 
                      style={styles.compareButton}
                      onPress={() => handleCompare(item.beforeImage!, item.afterImage!)}
                    >
                      <Ionicons name="resize-outline" size={16} color={theme.colors.system.blue} />
                      <Text style={styles.compareButtonText}>Compare</Text>
                    </TouchableOpacity>
                  </View>
                )}

                {/* Adjustments */}
                {item.adjustments && renderAdjustments(item.adjustments)}

                {/* Action Buttons */}
                <View style={styles.actionButtons}>
                  {item.isRevertable && (
                    <TouchableOpacity 
                      style={styles.revertButton}
                      onPress={() => handleRevert(item)}
                    >
                      <Ionicons name="arrow-undo-outline" size={16} color={theme.colors.system.red} />
                      <Text style={styles.revertButtonText}>Revert</Text>
                    </TouchableOpacity>
                  )}
                  
                  <TouchableOpacity style={styles.moreButton}>
                    <Ionicons name="ellipsis-vertical" size={16} color={theme.colors.text.tertiary} />
                  </TouchableOpacity>
                </View>
              </TouchableOpacity>
            </View>
          ))}
        </ScrollView>

        {/* Compare Modal */}
        {compareMode && compareImages && (
          <Animated.View 
            style={[
              styles.compareOverlay,
              {
                opacity: compareAnim,
                transform: [
                  { 
                    scale: compareAnim.interpolate({
                      inputRange: [0, 1],
                      outputRange: [0.8, 1],
                    })
                  }
                ]
              }
            ]}
          >
            <View style={styles.compareContainer}>
              <View style={styles.compareHeader}>
                <Text style={styles.compareTitle}>Before & After Comparison</Text>
                <TouchableOpacity onPress={handleCloseCompare}>
                  <Ionicons name="close" size={24} color={theme.colors.text.secondary} />
                </TouchableOpacity>
              </View>
              
              <View style={styles.compareImages}>
                <View style={styles.compareImageContainer}>
                  <Text style={styles.compareImageLabel}>Before</Text>
                  <Image source={{ uri: compareImages.before }} style={styles.compareImage} />
                </View>
                <View style={styles.compareImageContainer}>
                  <Text style={styles.compareImageLabel}>After</Text>
                  <Image source={{ uri: compareImages.after }} style={styles.compareImage} />
                </View>
              </View>
              
              <View style={styles.compareActions}>
                <TouchableOpacity style={styles.compareActionButton}>
                  <Ionicons name="download-outline" size={20} color={theme.colors.text.primary} />
                  <Text style={styles.compareActionText}>Export Before</Text>
                </TouchableOpacity>
                <TouchableOpacity style={styles.compareActionButton}>
                  <Ionicons name="download-outline" size={20} color={theme.colors.text.primary} />
                  <Text style={styles.compareActionText}>Export After</Text>
                </TouchableOpacity>
              </View>
            </View>
          </Animated.View>
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
  historyCount: {
    backgroundColor: theme.colors.lightGray,
    paddingHorizontal: theme.spacing.xs,
    paddingVertical: 2,
    borderRadius: theme.borderRadius.s,
  },
  historyCountText: {
    fontSize: theme.fontSizes.caption,
    color: theme.colors.text.secondary,
    fontWeight: '600',
  },
  closeButton: {
    padding: theme.spacing.s,
  },
  toggleSection: {
    padding: theme.spacing.l,
    borderBottomWidth: 1,
    borderBottomColor: theme.colors.border,
  },
  toggleButton: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'center',
    paddingVertical: theme.spacing.m,
    paddingHorizontal: theme.spacing.l,
    backgroundColor: theme.colors.lightGray,
    borderRadius: theme.borderRadius.m,
    gap: theme.spacing.s,
  },
  toggleButtonActive: {
    backgroundColor: theme.colors.green,
  },
  toggleButtonText: {
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.text.secondary,
  },
  toggleButtonTextActive: {
    color: theme.colors.surface,
  },
  historyContainer: {
    flex: 1,
    paddingHorizontal: theme.spacing.l,
  },
  historyItem: {
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
  historyContent: {
    flex: 1,
    backgroundColor: theme.colors.lightGray,
    borderRadius: theme.borderRadius.m,
    padding: theme.spacing.m,
    borderWidth: 2,
    borderColor: 'transparent',
  },
  actionHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'flex-start',
    marginBottom: theme.spacing.m,
  },
  actionLeft: {
    flexDirection: 'row',
    alignItems: 'flex-start',
    flex: 1,
    gap: theme.spacing.s,
  },
  actionIcon: {
    width: 32,
    height: 32,
    borderRadius: 16,
    justifyContent: 'center',
    alignItems: 'center',
  },
  actionInfo: {
    flex: 1,
  },
  actionDescription: {
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.text.primary,
    marginBottom: 2,
  },
  actionTimestamp: {
    fontSize: theme.fontSizes.caption,
    color: theme.colors.text.tertiary,
    fontWeight: '500',
  },
  presetBadge: {
    backgroundColor: theme.colors.green + '20',
    paddingHorizontal: theme.spacing.xs,
    paddingVertical: 2,
    borderRadius: theme.borderRadius.s,
  },
  presetText: {
    fontSize: theme.fontSizes.caption,
    fontWeight: '600',
    color: theme.colors.green,
  },
  beforeAfterContainer: {
    flexDirection: 'row',
    gap: theme.spacing.s,
    marginBottom: theme.spacing.m,
    alignItems: 'flex-end',
  },
  imageContainer: {
    flex: 1,
    alignItems: 'center',
  },
  imageLabel: {
    fontSize: theme.fontSizes.caption,
    fontWeight: '600',
    color: theme.colors.text.secondary,
    marginBottom: theme.spacing.xs,
  },
  historyImage: {
    width: 80,
    height: 60,
    borderRadius: theme.borderRadius.s,
  },
  compareButton: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: theme.spacing.s,
    paddingVertical: theme.spacing.xs,
    backgroundColor: theme.colors.system.blue + '20',
    borderRadius: theme.borderRadius.s,
    gap: 2,
  },
  compareButtonText: {
    fontSize: theme.fontSizes.caption,
    fontWeight: '600',
    color: theme.colors.system.blue,
  },
  adjustmentsContainer: {
    marginBottom: theme.spacing.m,
  },
  adjustmentItem: {
    flexDirection: 'row',
    alignItems: 'center',
    marginBottom: theme.spacing.xs,
    gap: theme.spacing.s,
  },
  adjustmentLabel: {
    fontSize: theme.fontSizes.caption,
    fontWeight: '600',
    color: theme.colors.text.secondary,
    width: 80,
  },
  adjustmentBar: {
    flex: 1,
    height: 4,
    backgroundColor: theme.colors.lightGray,
    borderRadius: 2,
    overflow: 'hidden',
    position: 'relative',
  },
  adjustmentFill: {
    position: 'absolute',
    height: '100%',
    borderRadius: 2,
  },
  adjustmentValue: {
    fontSize: theme.fontSizes.caption,
    fontWeight: '600',
    color: theme.colors.text.primary,
    width: 30,
    textAlign: 'right',
  },
  actionButtons: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
  },
  revertButton: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: theme.spacing.m,
    paddingVertical: theme.spacing.xs,
    backgroundColor: theme.colors.system.red + '10',
    borderRadius: theme.borderRadius.s,
    gap: theme.spacing.xs,
  },
  revertButtonText: {
    fontSize: theme.fontSizes.caption,
    fontWeight: '600',
    color: theme.colors.system.red,
  },
  moreButton: {
    padding: theme.spacing.xs,
  },
  compareOverlay: {
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
  compareContainer: {
    backgroundColor: theme.colors.surface,
    borderRadius: theme.borderRadius.xl,
    width: '90%',
    maxHeight: '80%',
    ...theme.shadows.large,
  },
  compareHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    padding: theme.spacing.l,
    borderBottomWidth: 1,
    borderBottomColor: theme.colors.border,
  },
  compareTitle: {
    fontSize: theme.fontSizes.title,
    fontWeight: '700',
    color: theme.colors.text.primary,
  },
  compareImages: {
    flexDirection: 'row',
    padding: theme.spacing.l,
    gap: theme.spacing.l,
  },
  compareImageContainer: {
    flex: 1,
    alignItems: 'center',
  },
  compareImageLabel: {
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.text.primary,
    marginBottom: theme.spacing.s,
  },
  compareImage: {
    width: 150,
    height: 120,
    borderRadius: theme.borderRadius.m,
  },
  compareActions: {
    flexDirection: 'row',
    padding: theme.spacing.l,
    gap: theme.spacing.m,
    borderTopWidth: 1,
    borderTopColor: theme.colors.border,
  },
  compareActionButton: {
    flex: 1,
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'center',
    paddingVertical: theme.spacing.m,
    paddingHorizontal: theme.spacing.m,
    backgroundColor: theme.colors.lightGray,
    borderRadius: theme.borderRadius.m,
    gap: theme.spacing.s,
  },
  compareActionText: {
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.text.primary,
  },
});

export default EditHistoryPanel;
