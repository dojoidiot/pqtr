import React, { useEffect, useRef } from 'react';
import { View, Text, TouchableOpacity, Animated, StyleSheet, Dimensions } from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import * as Haptics from 'expo-haptics';
import { theme } from '../constants/theme';

export type UploadItem = {
  id: string;
  filename: string;
  progress: number; // 0-100
  status: 'uploading' | 'success' | 'error';
};

interface LiveUploadQueueProps {
  uploads: UploadItem[];
  onClose?: () => void;
  visible: boolean;
}

const { width: screenWidth } = Dimensions.get('window');

const LiveUploadQueue: React.FC<LiveUploadQueueProps> = ({ uploads, onClose, visible }) => {
  const slideAnim = useRef(new Animated.Value(-300)).current;
  const fadeAnim = useRef(new Animated.Value(0)).current;

  useEffect(() => {
    if (visible) {
      Animated.parallel([
        Animated.spring(slideAnim, {
          toValue: 0,
          tension: 50,
          friction: 8,
          useNativeDriver: true,
        }),
        Animated.timing(fadeAnim, {
          toValue: 1,
          duration: 300,
          useNativeDriver: true,
        }),
      ]).start();
    } else {
      Animated.parallel([
        Animated.spring(slideAnim, {
          toValue: -300,
          tension: 50,
          friction: 8,
          useNativeDriver: true,
        }),
        Animated.timing(fadeAnim, {
          toValue: 0,
          duration: 200,
          useNativeDriver: true,
        }),
      ]).start();
    }
  }, [visible, slideAnim, fadeAnim]);

  const handleClose = () => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    onClose?.();
  };

  const getStatusIcon = (status: UploadItem['status']) => {
    switch (status) {
      case 'uploading':
        return <Ionicons name="cloud-upload" size={16} color={theme.colors.green} />;
      case 'success':
        return <Ionicons name="checkmark-circle" size={16} color={theme.colors.green} />;
      case 'error':
        return <Ionicons name="close-circle" size={16} color={theme.colors.semantic.error} />;
    }
  };

  const getStatusText = (status: UploadItem['status']) => {
    switch (status) {
      case 'uploading':
        return 'Uploading...';
      case 'success':
        return 'Uploaded';
      case 'error':
        return 'Failed';
    }
  };

  if (!visible) return null;

  return (
    <Animated.View style={[styles.overlay, { opacity: fadeAnim }]}>
      <Animated.View 
        style={[
          styles.container,
          { transform: [{ translateY: slideAnim }] }
        ]}
      >
        {/* Header */}
        <View style={styles.header}>
          <View style={styles.headerContent}>
            <Ionicons name="cloud-upload" size={20} color={theme.colors.accent} />
            <Text style={styles.headerTitle}>Live Upload Queue</Text>
            <Text style={styles.uploadCount}>{uploads.length} files</Text>
          </View>
          <TouchableOpacity
            style={styles.closeButton}
            onPress={handleClose}
            accessibilityLabel="Close upload queue"
            accessibilityRole="button"
          >
            <Ionicons name="close" size={20} color={theme.colors.gray} />
          </TouchableOpacity>
        </View>

        {/* Upload Items */}
        <View style={styles.uploadsList}>
          {uploads.length === 0 ? (
            <View style={styles.emptyState}>
              <Ionicons name="cloud-done" size={32} color={theme.colors.gray} />
              <Text style={styles.emptyText}>No active uploads</Text>
            </View>
          ) : (
            uploads.map((upload) => (
              <View key={upload.id} style={styles.uploadItem}>
                <View style={styles.uploadInfo}>
                  <View style={styles.uploadHeader}>
                    {getStatusIcon(upload.status)}
                    <Text style={styles.filename} numberOfLines={1}>
                      {upload.filename}
                    </Text>
                  </View>
                  <Text style={styles.statusText}>
                    {getStatusText(upload.status)}
                  </Text>
                </View>
                
                <View style={styles.progressContainer}>
                  <View style={styles.progressBar}>
                    <Animated.View 
                      style={[
                        styles.progressFill,
                        { width: `${upload.progress}%` }
                      ]} 
                    />
                  </View>
                  <Text style={styles.progressText}>
                    {upload.progress}%
                  </Text>
                </View>
              </View>
            ))
          )}
        </View>

        {/* Footer */}
        <View style={styles.footer}>
          <Text style={styles.footerText}>
            Files are automatically processed and added to your project
          </Text>
        </View>
      </Animated.View>
    </Animated.View>
  );
};

const styles = StyleSheet.create({
  overlay: {
    position: 'absolute',
    top: 0,
    left: 0,
    right: 0,
    bottom: 0,
    backgroundColor: 'rgba(0, 0, 0, 0.5)',
    zIndex: 2000,
    justifyContent: 'flex-end',
  },
  container: {
    backgroundColor: theme.colors.surface,
    borderTopLeftRadius: 20,
    borderTopRightRadius: 20,
    paddingTop: 20,
    maxHeight: '80%',
    shadowColor: '#000',
    shadowOffset: { width: 0, height: -4 },
    shadowOpacity: 0.3,
    shadowRadius: 12,
    elevation: 8,
  },
  header: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingHorizontal: 20,
    paddingBottom: 16,
    borderBottomWidth: 1,
    borderBottomColor: 'rgba(0, 0, 0, 0.1)',
  },
  headerContent: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 8,
  },
  headerTitle: {
    fontSize: 18,
    fontWeight: '700',
    color: theme.colors.accent,
  },
  uploadCount: {
    fontSize: 14,
    fontWeight: '600',
    color: theme.colors.gray,
    backgroundColor: 'rgba(0, 0, 0, 0.05)',
    paddingHorizontal: 8,
    paddingVertical: 4,
    borderRadius: 12,
  },
  closeButton: {
    width: 32,
    height: 32,
    borderRadius: 16,
    backgroundColor: 'rgba(0, 0, 0, 0.05)',
    justifyContent: 'center',
    alignItems: 'center',
  },
  uploadsList: {
    paddingHorizontal: 20,
    paddingVertical: 16,
    maxHeight: 400,
  },
  uploadItem: {
    marginBottom: 16,
    padding: 16,
    backgroundColor: 'rgba(0, 0, 0, 0.02)',
    borderRadius: 12,
    borderWidth: 1,
    borderColor: 'rgba(0, 0, 0, 0.05)',
  },
  uploadInfo: {
    marginBottom: 12,
  },
  uploadHeader: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 8,
    marginBottom: 4,
  },
  filename: {
    fontSize: 14,
    fontWeight: '600',
    color: theme.colors.accent,
    flex: 1,
  },
  statusText: {
    fontSize: 12,
    color: theme.colors.gray,
    marginLeft: 24,
  },
  progressContainer: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 12,
  },
  progressBar: {
    flex: 1,
    height: 6,
    backgroundColor: 'rgba(0, 0, 0, 0.1)',
    borderRadius: 3,
    overflow: 'hidden',
  },
  progressFill: {
    height: '100%',
    backgroundColor: theme.colors.green,
    borderRadius: 3,
  },
  progressText: {
    fontSize: 12,
    fontWeight: '600',
    color: theme.colors.accent,
    minWidth: 35,
    textAlign: 'right',
  },
  emptyState: {
    alignItems: 'center',
    paddingVertical: 40,
  },
  emptyText: {
    fontSize: 16,
    color: theme.colors.gray,
    marginTop: 12,
  },
  footer: {
    paddingHorizontal: 20,
    paddingVertical: 16,
    borderTopWidth: 1,
    borderTopColor: 'rgba(0, 0, 0, 0.1)',
  },
  footerText: {
    fontSize: 12,
    color: theme.colors.gray,
    textAlign: 'center',
    lineHeight: 16,
  },
});

export default LiveUploadQueue;
