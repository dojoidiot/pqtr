import React, { useState, useEffect, useRef } from 'react';
import {
  View,
  Text,
  TouchableOpacity,
  StyleSheet,
  Animated,
  Dimensions,
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import * as Haptics from 'expo-haptics';
import { theme } from '../constants/theme';

const { width: screenWidth } = Dimensions.get('window');

interface SyncItem {
  id: string;
  type: 'upload' | 'download' | 'delete' | 'update';
  filename: string;
  size: string;
  status: 'pending' | 'syncing' | 'completed' | 'failed';
  progress: number;
  error?: string;
}

interface SyncStatusBannerProps {
  isVisible: boolean;
  onClose: () => void;
  onRetry: () => void;
  onViewDetails: () => void;
}

const SyncStatusBanner: React.FC<SyncStatusBannerProps> = ({
  isVisible,
  onClose,
  onRetry,
  onViewDetails,
}) => {
  const [syncItems, setSyncItems] = useState<SyncItem[]>([]);
  const [isOnline, setIsOnline] = useState(true);
  const [isExpanded, setIsExpanded] = useState(false);
  const [showToast, setShowToast] = useState(false);
  const [toastMessage, setToastMessage] = useState('');
  
  const slideAnim = useRef(new Animated.Value(-100)).current;
  const expandAnim = useRef(new Animated.Value(0)).current;
  const toastAnim = useRef(new Animated.Value(0)).current;

  useEffect(() => {
    if (isVisible) {
      // Slide in animation
      Animated.spring(slideAnim, {
        toValue: 0,
        useNativeDriver: true,
        tension: 100,
        friction: 8,
      }).start();
      
      loadSyncQueue();
      startSyncProcess();
    } else {
      // Slide out animation
      Animated.spring(slideAnim, {
        toValue: -100,
        useNativeDriver: true,
        tension: 100,
        friction: 8,
      }).start();
    }
  }, [isVisible]);

  useEffect(() => {
    // Simulate network status changes
    const networkInterval = setInterval(() => {
      setIsOnline(Math.random() > 0.1); // 90% chance of being online
    }, 10000);

    return () => clearInterval(networkInterval);
  }, []);

  const loadSyncQueue = () => {
    // Mock sync queue data
    const mockItems: SyncItem[] = [
      {
        id: '1',
        type: 'upload',
        filename: 'IMG_001.jpg',
        size: '24.5 MB',
        status: 'pending',
        progress: 0,
      },
      {
        id: '2',
        type: 'upload',
        filename: 'IMG_002.jpg',
        size: '18.2 MB',
        status: 'syncing',
        progress: 45,
      },
      {
        id: '3',
        type: 'download',
        filename: 'preset_vivid.json',
        size: '2.1 KB',
        status: 'completed',
        progress: 100,
      },
      {
        id: '4',
        type: 'update',
        filename: 'IMG_003.jpg',
        size: '22.8 MB',
        status: 'failed',
        progress: 0,
        error: 'Network timeout',
      },
    ];
    
    setSyncItems(mockItems);
  };

  const startSyncProcess = () => {
    // Simulate sync process
    const syncInterval = setInterval(() => {
      setSyncItems(prev => {
        const updated = prev.map(item => {
          if (item.status === 'pending' && isOnline) {
            return { ...item, status: 'syncing' as const, progress: 0 };
          }
          if (item.status === 'syncing') {
            const newProgress = Math.min(item.progress + Math.random() * 30, 100);
            if (newProgress >= 100) {
              return { ...item, status: 'completed' as const, progress: 100 };
            }
            return { ...item, progress: newProgress };
          }
          return item;
        });
        
        // Check if all items are completed
        const allCompleted = updated.every(item => item.status === 'completed');
        if (allCompleted && prev.some(item => item.status === 'syncing')) {
          showSyncCompleteToast();
        }
        
        return updated;
      });
    }, 2000);

    return () => clearInterval(syncInterval);
  };

  const showSyncCompleteToast = () => {
    setToastMessage('All items synced successfully! 🎉');
    setShowToast(true);
    
    Animated.spring(toastAnim, {
      toValue: 1,
      useNativeDriver: true,
      tension: 100,
      friction: 8,
    }).start();
    
    setTimeout(() => {
      Animated.spring(toastAnim, {
        toValue: 0,
        useNativeDriver: true,
        tension: 100,
        friction: 8,
      }).start(() => setShowToast(false));
    }, 3000);
  };

  const handleExpandToggle = () => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    setIsExpanded(!isExpanded);
    
    Animated.spring(expandAnim, {
      toValue: isExpanded ? 0 : 1,
      useNativeDriver: false,
      tension: 100,
      friction: 8,
    }).start();
  };

  const handleRetry = () => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Medium);
    onRetry();
  };

  const handleViewDetails = () => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    onViewDetails();
  };

  const getStatusIcon = (status: SyncItem['status']) => {
    switch (status) {
      case 'pending': return 'time-outline';
      case 'syncing': return 'sync-outline';
      case 'completed': return 'checkmark-circle-outline';
      case 'failed': return 'close-circle-outline';
      default: return 'ellipse-outline';
    }
  };

  const getStatusColor = (status: SyncItem['status']) => {
    switch (status) {
      case 'pending': return theme.colors.system.orange;
      case 'syncing': return theme.colors.system.blue;
      case 'completed': return theme.colors.system.green;
      case 'failed': return theme.colors.system.red;
      default: return theme.colors.gray;
    }
  };

  const getTypeIcon = (type: SyncItem['type']) => {
    switch (type) {
      case 'upload': return 'cloud-upload-outline';
      case 'download': return 'cloud-download-outline';
      case 'delete': return 'trash-outline';
      case 'update': return 'refresh-outline';
      default: return 'document-outline';
    }
  };

  const getTypeColor = (type: SyncItem['type']) => {
    switch (type) {
      case 'upload': return theme.colors.system.blue;
      case 'download': return theme.colors.system.green;
      case 'delete': return theme.colors.system.red;
      case 'update': return theme.colors.system.orange;
      default: return theme.colors.gray;
    }
  };

  const pendingCount = syncItems.filter(item => item.status === 'pending').length;
  const syncingCount = syncItems.filter(item => item.status === 'syncing').length;
  const failedCount = syncItems.filter(item => item.status === 'failed').length;

  if (!isVisible) return null;

  return (
    <>
      {/* Main Banner */}
      <Animated.View 
        style={[
          styles.banner,
          {
            transform: [{ translateY: slideAnim }]
          }
        ]}
      >
        {/* Status Bar */}
        <View style={styles.statusBar}>
          <View style={styles.statusLeft}>
            <View style={[styles.statusIndicator, { backgroundColor: isOnline ? theme.colors.system.green : theme.colors.system.red }]} />
            <Text style={styles.statusText}>
              {isOnline ? 'Online' : 'Offline'}
            </Text>
          </View>
          
          <View style={styles.statusRight}>
            {pendingCount > 0 && (
              <View style={styles.statusBadge}>
                <Text style={styles.statusBadgeText}>{pendingCount}</Text>
              </View>
            )}
            {failedCount > 0 && (
              <View style={[styles.statusBadge, styles.failedBadge]}>
                <Text style={styles.statusBadgeText}>{failedCount}</Text>
              </View>
            )}
          </View>
        </View>

        {/* Main Content */}
        <View style={styles.mainContent}>
          <View style={styles.contentLeft}>
            <Ionicons 
              name={syncingCount > 0 ? "sync" : "cloud-done-outline"} 
              size={24} 
              color={syncingCount > 0 ? theme.colors.system.blue : theme.colors.system.green} 
            />
            <View style={styles.contentText}>
              <Text style={styles.title}>
                {syncingCount > 0 ? 'Syncing...' : 'Sync Complete'}
              </Text>
              <Text style={styles.subtitle}>
                {pendingCount > 0 
                  ? `${pendingCount} items pending` 
                  : syncingCount > 0 
                    ? `${syncingCount} items syncing` 
                    : 'All items up to date'
                }
              </Text>
            </View>
          </View>
          
          <View style={styles.contentRight}>
            <TouchableOpacity 
              style={styles.expandButton}
              onPress={handleExpandToggle}
            >
              <Ionicons 
                name={isExpanded ? "chevron-up" : "chevron-down"} 
                size={20} 
                color={theme.colors.text.secondary} 
              />
            </TouchableOpacity>
            
            <TouchableOpacity 
              style={styles.closeButton}
              onPress={onClose}
            >
              <Ionicons name="close" size={20} color={theme.colors.text.secondary} />
            </TouchableOpacity>
          </View>
        </View>

        {/* Progress Bar */}
        {syncingCount > 0 && (
          <View style={styles.progressContainer}>
            <View style={styles.progressBar}>
              <View 
                style={[
                  styles.progressFill, 
                  { 
                    width: `${syncItems.reduce((acc, item) => acc + item.progress, 0) / syncItems.length}%` 
                  }
                ]} 
              />
            </View>
            <Text style={styles.progressText}>
              {Math.round(syncItems.reduce((acc, item) => acc + item.progress, 0) / syncItems.length)}%
            </Text>
          </View>
        )}

        {/* Expandable Details */}
        <Animated.View 
          style={[
            styles.expandableContent,
            {
              maxHeight: expandAnim.interpolate({
                inputRange: [0, 1],
                outputRange: [0, 300],
              }),
              opacity: expandAnim,
            }
          ]}
        >
          <View style={styles.detailsHeader}>
            <Text style={styles.detailsTitle}>Sync Queue</Text>
            <TouchableOpacity style={styles.viewDetailsButton} onPress={handleViewDetails}>
              <Text style={styles.viewDetailsText}>View All</Text>
            </TouchableOpacity>
          </View>
          
          {syncItems.slice(0, 3).map((item) => (
            <View key={item.id} style={styles.syncItem}>
              <View style={styles.itemLeft}>
                <View style={[styles.itemTypeIcon, { backgroundColor: getTypeColor(item.type) + '20' }]}>
                  <Ionicons name={getTypeIcon(item.type)} size={16} color={getTypeColor(item.type)} />
                </View>
                <View style={styles.itemInfo}>
                  <Text style={styles.itemFilename} numberOfLines={1}>
                    {item.filename}
                  </Text>
                  <Text style={styles.itemSize}>{item.size}</Text>
                </View>
              </View>
              
              <View style={styles.itemRight}>
                <View style={styles.itemStatus}>
                  <Ionicons 
                    name={getStatusIcon(item.status)} 
                    size={16} 
                    color={getStatusColor(item.status)} 
                  />
                  <Text style={[styles.itemStatusText, { color: getStatusColor(item.status) }]}>
                    {item.status}
                  </Text>
                </View>
                
                {item.status === 'syncing' && (
                  <View style={styles.itemProgress}>
                    <View style={styles.itemProgressBar}>
                      <View style={[styles.itemProgressFill, { width: `${item.progress}%` }]} />
                    </View>
                    <Text style={styles.itemProgressText}>{Math.round(item.progress)}%</Text>
                  </View>
                )}
                
                {item.status === 'failed' && (
                  <TouchableOpacity style={styles.retryButton} onPress={handleRetry}>
                    <Ionicons name="refresh" size={16} color={theme.colors.system.red} />
                  </TouchableOpacity>
                )}
              </View>
            </View>
          ))}
          
          {syncItems.length > 3 && (
            <View style={styles.moreItems}>
              <Text style={styles.moreItemsText}>
                +{syncItems.length - 3} more items
              </Text>
            </View>
          )}
        </Animated.View>
      </Animated.View>

      {/* Toast Notification */}
      {showToast && (
        <Animated.View 
          style={[
            styles.toast,
            {
              transform: [
                { 
                  translateY: toastAnim.interpolate({
                    inputRange: [0, 1],
                    outputRange: [100, 0],
                  })
                },
                { 
                  scale: toastAnim.interpolate({
                    inputRange: [0, 1],
                    outputRange: [0.8, 1],
                  })
                }
              ],
              opacity: toastAnim,
            }
          ]}
        >
          <Ionicons name="checkmark-circle" size={20} color={theme.colors.system.green} />
          <Text style={styles.toastText}>{toastMessage}</Text>
        </Animated.View>
      )}
    </>
  );
};

const styles = StyleSheet.create({
  banner: {
    position: 'absolute',
    top: 0,
    left: 0,
    right: 0,
    backgroundColor: theme.colors.surface,
    borderBottomWidth: 1,
    borderBottomColor: theme.colors.border,
    ...theme.shadows.medium,
    zIndex: 1000,
  },
  statusBar: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingHorizontal: theme.spacing.m,
    paddingVertical: theme.spacing.xs,
    backgroundColor: theme.colors.lightGray,
  },
  statusLeft: {
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
  statusRight: {
    flexDirection: 'row',
    gap: theme.spacing.xs,
  },
  statusBadge: {
    backgroundColor: theme.colors.system.orange,
    paddingHorizontal: theme.spacing.xs,
    paddingVertical: 2,
    borderRadius: theme.borderRadius.s,
    minWidth: 20,
    alignItems: 'center',
  },
  failedBadge: {
    backgroundColor: theme.colors.system.red,
  },
  statusBadgeText: {
    fontSize: theme.fontSizes.caption,
    fontWeight: '700',
    color: theme.colors.surface,
  },
  mainContent: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    padding: theme.spacing.m,
  },
  contentLeft: {
    flexDirection: 'row',
    alignItems: 'center',
    flex: 1,
    gap: theme.spacing.m,
  },
  contentText: {
    flex: 1,
  },
  title: {
    fontSize: theme.fontSizes.headline,
    fontWeight: '700',
    color: theme.colors.text.primary,
    marginBottom: 2,
  },
  subtitle: {
    fontSize: theme.fontSizes.body,
    color: theme.colors.text.secondary,
    fontWeight: '500',
  },
  contentRight: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: theme.spacing.s,
  },
  expandButton: {
    padding: theme.spacing.s,
  },
  closeButton: {
    padding: theme.spacing.s,
  },
  progressContainer: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: theme.spacing.m,
    paddingBottom: theme.spacing.m,
    gap: theme.spacing.m,
  },
  progressBar: {
    flex: 1,
    height: 4,
    backgroundColor: theme.colors.lightGray,
    borderRadius: 2,
    overflow: 'hidden',
  },
  progressFill: {
    height: '100%',
    backgroundColor: theme.colors.system.blue,
    borderRadius: 2,
  },
  progressText: {
    fontSize: theme.fontSizes.caption,
    fontWeight: '600',
    color: theme.colors.text.secondary,
    minWidth: 30,
    textAlign: 'right',
  },
  expandableContent: {
    overflow: 'hidden',
    borderTopWidth: 1,
    borderTopColor: theme.colors.border,
  },
  detailsHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    padding: theme.spacing.m,
    paddingBottom: theme.spacing.s,
  },
  detailsTitle: {
    fontSize: theme.fontSizes.body,
    fontWeight: '700',
    color: theme.colors.text.primary,
  },
  viewDetailsButton: {
    paddingHorizontal: theme.spacing.m,
    paddingVertical: theme.spacing.xs,
    backgroundColor: theme.colors.lightGray,
    borderRadius: theme.borderRadius.m,
  },
  viewDetailsText: {
    fontSize: theme.fontSizes.caption,
    fontWeight: '600',
    color: theme.colors.text.secondary,
  },
  syncItem: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    paddingHorizontal: theme.spacing.m,
    paddingVertical: theme.spacing.s,
    borderBottomWidth: 1,
    borderBottomColor: theme.colors.border,
  },
  itemLeft: {
    flexDirection: 'row',
    alignItems: 'center',
    flex: 1,
    gap: theme.spacing.s,
  },
  itemTypeIcon: {
    width: 32,
    height: 32,
    borderRadius: 16,
    justifyContent: 'center',
    alignItems: 'center',
  },
  itemInfo: {
    flex: 1,
  },
  itemFilename: {
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.text.primary,
    marginBottom: 2,
  },
  itemSize: {
    fontSize: theme.fontSizes.caption,
    color: theme.colors.text.tertiary,
    fontWeight: '500',
  },
  itemRight: {
    alignItems: 'flex-end',
    gap: theme.spacing.xs,
  },
  itemStatus: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: theme.spacing.xs,
  },
  itemStatusText: {
    fontSize: theme.fontSizes.caption,
    fontWeight: '600',
  },
  itemProgress: {
    alignItems: 'flex-end',
    gap: theme.spacing.xs,
  },
  itemProgressBar: {
    width: 60,
    height: 3,
    backgroundColor: theme.colors.lightGray,
    borderRadius: 1.5,
    overflow: 'hidden',
  },
  itemProgressFill: {
    height: '100%',
    backgroundColor: theme.colors.system.blue,
    borderRadius: 1.5,
  },
  itemProgressText: {
    fontSize: theme.fontSizes.caption,
    fontWeight: '600',
    color: theme.colors.text.secondary,
  },
  retryButton: {
    padding: theme.spacing.xs,
    backgroundColor: theme.colors.system.red + '10',
    borderRadius: theme.borderRadius.s,
  },
  moreItems: {
    padding: theme.spacing.m,
    alignItems: 'center',
  },
  moreItemsText: {
    fontSize: theme.fontSizes.caption,
    color: theme.colors.text.tertiary,
    fontWeight: '500',
  },
  toast: {
    position: 'absolute',
    bottom: 100,
    left: theme.spacing.l,
    right: theme.spacing.l,
    backgroundColor: theme.colors.surface,
    borderRadius: theme.borderRadius.l,
    padding: theme.spacing.m,
    flexDirection: 'row',
    alignItems: 'center',
    gap: theme.spacing.s,
    ...theme.shadows.large,
    zIndex: 1001,
  },
  toastText: {
    flex: 1,
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.text.primary,
  },
});

export default SyncStatusBanner;
