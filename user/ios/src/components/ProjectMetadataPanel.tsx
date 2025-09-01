import React, { useState, useRef } from 'react';
import { 
  View, 
  Text, 
  TouchableOpacity, 
  StyleSheet, 
  Animated 
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import * as Haptics from 'expo-haptics';
import { theme } from '../constants/theme';

interface ProjectMetadata {
  title: string;
  photoCount: number;
  publishedCount: number;
  uneditedCount: number;
  presetName: string;
  camera: string;
  client?: string;
  location: string;
}

interface ProjectMetadataPanelProps {
  metadata: ProjectMetadata;
  isExpanded?: boolean;
  onToggle?: () => void;
}

const ProjectMetadataPanel: React.FC<ProjectMetadataPanelProps> = ({
  metadata,
  isExpanded = false,
  onToggle
}) => {
  const [expanded, setExpanded] = useState(isExpanded);
  const rotateAnim = useRef(new Animated.Value(isExpanded ? 1 : 0)).current;
  const heightAnim = useRef(new Animated.Value(isExpanded ? 1 : 0)).current;

  const handleToggle = () => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    setExpanded(!expanded);
    onToggle?.();
    
    Animated.parallel([
      Animated.spring(rotateAnim, {
        toValue: expanded ? 0 : 1,
        useNativeDriver: true,
        tension: 100,
        friction: 8,
      }),
      Animated.timing(heightAnim, {
        toValue: expanded ? 0 : 1,
        duration: 300,
        useNativeDriver: false,
      }),
    ]).start();
  };

  const renderMetadataRow = (label: string, value: string | number, icon?: string) => (
    <View style={styles.metadataRow}>
      <View style={styles.metadataLeft}>
        {icon && (
          <Ionicons 
            name={icon as any} 
            size={16} 
            color={theme.colors.text.secondary} 
          />
        )}
        <Text style={styles.metadataLabel}>{label}</Text>
      </View>
      <Text style={styles.metadataValue}>{value}</Text>
    </View>
  );

  return (
    <View style={styles.container}>
      {/* Header */}
      <TouchableOpacity 
        style={styles.header}
        onPress={handleToggle}
        activeOpacity={0.8}
      >
        <View style={styles.headerLeft}>
          <Ionicons name="information-circle" size={20} color={theme.colors.semantic.primary} />
          <Text style={styles.headerTitle}>Project Info</Text>
        </View>
        
        <Animated.View
          style={{
            transform: [{
              rotate: rotateAnim.interpolate({
                inputRange: [0, 1],
                outputRange: ['0deg', '180deg'],
              })
            }]
          }}
        >
          <Ionicons 
            name="chevron-down" 
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
              outputRange: [0, 300],
            }),
            opacity: heightAnim,
          }
        ]}
      >
        {expanded && (
          <View style={styles.metadataContent}>
            {/* Project Title */}
            <View style={styles.projectTitle}>
              <Ionicons name="camera" size={24} color={theme.colors.semantic.primary} />
              <Text style={styles.projectTitleText}>{metadata.title}</Text>
            </View>

            {/* Stats Grid */}
            <View style={styles.statsGrid}>
              <View style={styles.statItem}>
                <Text style={styles.statNumber}>{metadata.photoCount}</Text>
                <Text style={styles.statLabel}>Photos</Text>
              </View>
              <View style={styles.statItem}>
                <Text style={styles.statNumber}>{metadata.publishedCount}</Text>
                <Text style={styles.statLabel}>Published</Text>
              </View>
              <View style={styles.statItem}>
                <Text style={styles.statNumber}>{metadata.uneditedCount}</Text>
                <Text style={styles.statLabel}>Unedited</Text>
              </View>
            </View>

            {/* Metadata Details */}
            <View style={styles.metadataDetails}>
              {renderMetadataRow('Preset', metadata.presetName, 'color-palette')}
              {renderMetadataRow('Camera', metadata.camera, 'camera')}
              {metadata.client && renderMetadataRow('Client', metadata.client, 'business')}
              {renderMetadataRow('Location', metadata.location, 'location')}
            </View>
          </View>
        )}
      </Animated.View>
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
    backgroundColor: 'rgba(0, 122, 255, 0.1)',
    borderBottomWidth: 1,
    borderBottomColor: 'rgba(0, 122, 255, 0.2)',
  },
  headerLeft: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: theme.spacing.s,
  },
  headerTitle: {
    fontSize: 16,
    fontWeight: '600',
    color: theme.colors.semantic.primary,
  },
  content: {
    overflow: 'hidden',
  },
  metadataContent: {
    padding: theme.spacing.m,
  },
  projectTitle: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: theme.spacing.s,
    marginBottom: theme.spacing.m,
  },
  projectTitleText: {
    fontSize: 20,
    fontWeight: '700',
    color: theme.colors.text.primary,
  },
  statsGrid: {
    flexDirection: 'row',
    justifyContent: 'space-around',
    marginBottom: theme.spacing.l,
    paddingVertical: theme.spacing.m,
    backgroundColor: 'rgba(0, 0, 0, 0.02)',
    borderRadius: theme.borderRadius.m,
  },
  statItem: {
    alignItems: 'center',
  },
  statNumber: {
    fontSize: 24,
    fontWeight: '700',
    color: theme.colors.semantic.primary,
  },
  statLabel: {
    fontSize: 12,
    fontWeight: '500',
    color: theme.colors.text.secondary,
    marginTop: 2,
  },
  metadataDetails: {
    gap: theme.spacing.s,
  },
  metadataRow: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    paddingVertical: theme.spacing.s,
    borderBottomWidth: 1,
    borderBottomColor: 'rgba(0, 0, 0, 0.05)',
  },
  metadataLeft: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: theme.spacing.s,
  },
  metadataLabel: {
    fontSize: 14,
    fontWeight: '500',
    color: theme.colors.text.secondary,
  },
  metadataValue: {
    fontSize: 14,
    fontWeight: '600',
    color: theme.colors.text.primary,
  },
});

export default ProjectMetadataPanel;
