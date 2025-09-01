import React, { useState, useRef, useEffect } from 'react';
import { 
  View, 
  Text, 
  TouchableOpacity, 
  StyleSheet, 
  Animated,
  FlatList
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import * as Haptics from 'expo-haptics';
import { theme } from '../constants/theme';

interface IngestionItem {
  id: string;
  filename: string;
  status: 'syncing' | 'applying-preset' | 'synced' | 'error';
  progress?: number;
  presetName?: string;
  timestamp: string;
}

interface LiveIngestionDrawerProps {
  isVisible: boolean;
  onToggle: () => void;
  items: IngestionItem[];
}

const LiveIngestionDrawer: React.FC<LiveIngestionDrawerProps> = ({
  isVisible,
  onToggle,
  items
}) => {
  const [isExpanded, setIsExpanded] = useState(false);
  const slideAnim = useRef(new Animated.Value(0)).current;
  const heightAnim = useRef(new Animated.Value(0)).current;

  useEffect(() => {
    if (isVisible) {
      Animated.parallel([
        Animated.spring(slideAnim, {
          toValue: 1,
          useNativeDriver: true,
          tension: 100,
          friction: 8,
        }),
        Animated.timing(heightAnim, {
          toValue: 1,
          duration: 300,
          useNativeDriver: false,
        }),
      ]).start();
    } else {
      Animated.parallel([
        Animated.spring(slideAnim, {
          toValue: 0,
          useNativeDriver: true,
          tension: 100,
          friction: 8,
        }),
        Animated.timing(heightAnim, {
          toValue: 0,
          duration: 200,
          useNativeDriver: false,
        }),
      ]).start();
    }
  }, [isVisible, slideAnim, heightAnim]);

  const handleToggle = () => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    setIsExpanded(!isExpanded);
    onToggle();
  };

  const getStatusIcon = (status: IngestionItem['status']) => {
    switch (status) {
      case 'syncing':
        return <Ionicons name="sync" size={16} color={theme.colors.semantic.warning} />;
      case 'applying-preset':
        return <Ionicons name="color-palette" size={16} color={theme.colors.semantic.info} />;
      case 'synced':
        return <Ionicons name="checkmark-circle" size={16} color={theme.colors.semantic.success} />;
      case 'error':
        return <Ionicons name="close-circle" size={16} color={theme.colors.semantic.error} />;
      default:
        return <Ionicons name="ellipse" size={16} color={theme.colors.text.tertiary} />;
    }
  };

  const getStatusText = (status: IngestionItem['status']) => {
    switch (status) {
      case 'syncing':
        return 'syncing';
      case 'applying-preset':
        return 'applying preset';
      case 'synced':
        return 'synced';
      case 'error':
        return 'error';
      default:
        return 'pending';
    }
  };

  const renderIngestionItem = ({ item }: { item: IngestionItem }) => (
    <View style={styles.ingestionItem}>
      <View style={styles.itemLeft}>
        {getStatusIcon(item.status)}
        <View style={styles.itemInfo}>
          <Text style={styles.filename} numberOfLines={1}>
            {item.filename}
          </Text>
          <Text style={styles.statusText}>
            {getStatusText(item.status)}
            {item.presetName && ` • ${item.presetName}`}
          </Text>
        </View>
      </View>
      
      {item.status === 'syncing' && item.progress !== undefined && (
        <View style={styles.progressContainer}>
          <View style={styles.progressBar}>
            <View 
              style={[
                styles.progressFill, 
                { width: `${item.progress}%` }
              ]} 
            />
          </View>
          <Text style={styles.progressText}>{item.progress}%</Text>
        </View>
      )}
    </View>
  );

  if (!isVisible) return null;

  return (
    <Animated.View 
      style={[
        styles.container,
        {
          transform: [
            {
              translateY: slideAnim.interpolate({
                inputRange: [0, 1],
                outputRange: [-20, 0],
              })
            }
          ],
          opacity: slideAnim,
        }
      ]}
    >
      {/* Header */}
      <TouchableOpacity 
        style={styles.header}
        onPress={handleToggle}
        activeOpacity={0.8}
      >
        <View style={styles.headerLeft}>
          <Ionicons name="camera" size={16} color={theme.colors.semantic.success} />
          <Text style={styles.headerTitle}>Live Upload Queue</Text>
          <View style={styles.statusBadge}>
            <Text style={styles.statusBadgeText}>
              {items.filter(item => item.status === 'synced').length}/{items.length}
            </Text>
          </View>
        </View>
        
        <Animated.View
          style={{
            transform: [{
              rotate: isExpanded ? '180deg' : '0deg'
            }]
          }}
        >
          <Ionicons 
            name="chevron-up" 
            size={20} 
            color={theme.colors.text.secondary} 
          />
        </Animated.View>
      </TouchableOpacity>

      {/* Content */}
      <Animated.View
        style={[
          styles.content,
          {
            maxHeight: heightAnim.interpolate({
              inputRange: [0, 1],
              outputRange: [0, 200],
            }),
          }
        ]}
      >
        {isExpanded && (
          <FlatList
            data={items}
            renderItem={renderIngestionItem}
            keyExtractor={(item) => item.id}
            showsVerticalScrollIndicator={false}
            contentContainerStyle={styles.listContent}
          />
        )}
      </Animated.View>
    </Animated.View>
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
    backgroundColor: 'rgba(52, 199, 89, 0.1)',
    borderBottomWidth: 1,
    borderBottomColor: 'rgba(52, 199, 89, 0.2)',
  },
  headerLeft: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: theme.spacing.s,
  },
  headerTitle: {
    fontSize: 14,
    fontWeight: '600',
    color: theme.colors.semantic.success,
  },
  statusBadge: {
    backgroundColor: theme.colors.semantic.success,
    paddingHorizontal: 8,
    paddingVertical: 2,
    borderRadius: 10,
  },
  statusBadgeText: {
    fontSize: 11,
    fontWeight: '700',
    color: theme.colors.surface,
  },
  content: {
    overflow: 'hidden',
  },
  listContent: {
    padding: theme.spacing.m,
  },
  ingestionItem: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    paddingVertical: theme.spacing.s,
    borderBottomWidth: 1,
    borderBottomColor: 'rgba(0, 0, 0, 0.05)',
  },
  itemLeft: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: theme.spacing.s,
    flex: 1,
  },
  itemInfo: {
    flex: 1,
  },
  filename: {
    fontSize: 13,
    fontWeight: '600',
    color: theme.colors.text.primary,
  },
  statusText: {
    fontSize: 11,
    color: theme.colors.text.secondary,
    marginTop: 2,
  },
  progressContainer: {
    alignItems: 'center',
    gap: 4,
  },
  progressBar: {
    width: 60,
    height: 4,
    backgroundColor: 'rgba(0, 0, 0, 0.1)',
    borderRadius: 2,
    overflow: 'hidden',
  },
  progressFill: {
    height: '100%',
    backgroundColor: theme.colors.semantic.warning,
    borderRadius: 2,
  },
  progressText: {
    fontSize: 10,
    fontWeight: '600',
    color: theme.colors.text.secondary,
  },
});

export default LiveIngestionDrawer;
