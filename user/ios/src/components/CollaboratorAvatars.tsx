import React, { useState, useEffect, useRef } from 'react';
import {
  View,
  Text,
  TouchableOpacity,
  StyleSheet,
  ScrollView,
  TextInput,
  Alert,
  Animated,
  Dimensions,
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import * as Haptics from 'expo-haptics';
import { theme } from '../constants/theme';

const { width: screenWidth } = Dimensions.get('window');

interface Collaborator {
  id: string;
  name: string;
  email: string;
  avatar: string;
  role: 'photographer' | 'editor' | 'admin' | 'viewer';
  status: 'online' | 'offline' | 'away';
  lastSeen: string;
  isActive: boolean;
  currentActivity?: string;
}

interface CollaboratorAvatarsProps {
  isVisible: boolean;
  onClose: () => void;
  onInvite: (email: string) => void;
  onRoleChange: (collaboratorId: string, newRole: Collaborator['role']) => void;
}

const CollaboratorAvatars: React.FC<CollaboratorAvatarsProps> = ({
  isVisible,
  onClose,
  onInvite,
  onRoleChange,
}) => {
  const [collaborators, setCollaborators] = useState<Collaborator[]>([]);
  const [inviteEmail, setInviteEmail] = useState('');
  const [showInviteForm, setShowInviteForm] = useState(false);
  const [selectedCollaborator, setSelectedCollaborator] = useState<Collaborator | null>(null);
  const [showQRCode, setShowQRCode] = useState(false);
  
  const slideAnim = useRef(new Animated.Value(300)).current;
  const fadeAnim = useRef(new Animated.Value(0)).current;
  const pulseAnim = useRef(new Animated.Value(1)).current;

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
      
      loadCollaborators();
      startPresenceSimulation();
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

  useEffect(() => {
    // Start pulsing animation for online users
    if (collaborators.some(c => c.status === 'online')) {
      Animated.loop(
        Animated.sequence([
          Animated.timing(pulseAnim, {
            toValue: 1.1,
            duration: 1000,
            useNativeDriver: true,
          }),
          Animated.timing(pulseAnim, {
            toValue: 1,
            duration: 1000,
            useNativeDriver: true,
          }),
        ])
      ).start();
    }
  }, [collaborators]);

  const loadCollaborators = () => {
    const mockCollaborators: Collaborator[] = [
      {
        id: '1',
        name: 'Sarah Chen',
        email: 'sarah.chen@studio.com',
        avatar: 'SC',
        role: 'photographer',
        status: 'online',
        lastSeen: '2024-01-15T10:30:00Z',
        isActive: true,
        currentActivity: 'Editing Mountain Landscape',
      },
      {
        id: '2',
        name: 'Marcus Rodriguez',
        email: 'marcus.r@studio.com',
        avatar: 'MR',
        role: 'editor',
        status: 'online',
        lastSeen: '2024-01-15T10:25:00Z',
        isActive: true,
        currentActivity: 'Applying Vivid Preset',
      },
      {
        id: '3',
        name: 'Emma Thompson',
        email: 'emma.t@studio.com',
        avatar: 'ET',
        role: 'admin',
        status: 'away',
        lastSeen: '2024-01-15T10:15:00Z',
        isActive: false,
        currentActivity: 'Reviewing exports',
      },
      {
        id: '4',
        name: 'David Kim',
        email: 'david.kim@studio.com',
        avatar: 'DK',
        role: 'viewer',
        status: 'offline',
        lastSeen: '2024-01-15T09:45:00Z',
        isActive: false,
      },
    ];
    
    setCollaborators(mockCollaborators);
  };

  const startPresenceSimulation = () => {
    // Simulate real-time presence updates
    const presenceInterval = setInterval(() => {
      setCollaborators(prev => 
        prev.map(collaborator => {
          // Randomly change status for demo purposes
          if (Math.random() > 0.95) {
            const newStatus = ['online', 'away', 'offline'][Math.floor(Math.random() * 3)] as Collaborator['status'];
            return { ...collaborator, status: newStatus };
          }
          return collaborator;
        })
      );
    }, 5000);

    return () => clearInterval(presenceInterval);
  };

  const handleInvite = () => {
    if (!inviteEmail.trim()) {
      Alert.alert('Invalid Email', 'Please enter a valid email address.');
      return;
    }

    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Medium);
    onInvite(inviteEmail.trim());
    setInviteEmail('');
    setShowInviteForm(false);
    
    Alert.alert(
      'Invitation Sent', 
      `Invitation sent to ${inviteEmail.trim()}. They will receive an email with access instructions.`
    );
  };

  const handleRoleChange = (collaboratorId: string, newRole: Collaborator['role']) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Medium);
    onRoleChange(collaboratorId, newRole);
    
    setCollaborators(prev => 
      prev.map(collaborator => 
        collaborator.id === collaboratorId 
          ? { ...collaborator, role: newRole }
          : collaborator
      )
    );
  };

  const handleCollaboratorPress = (collaborator: Collaborator) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    setSelectedCollaborator(collaborator);
  };

  const handleShowQR = () => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Medium);
    setShowQRCode(true);
  };

  const getRoleColor = (role: Collaborator['role']) => {
    switch (role) {
      case 'photographer': return theme.colors.system.blue;
      case 'editor': return theme.colors.system.green;
      case 'admin': return theme.colors.system.purple;
      case 'viewer': return theme.colors.system.orange;
      default: return theme.colors.gray;
    }
  };

  const getRoleIcon = (role: Collaborator['role']) => {
    switch (role) {
      case 'photographer': return 'camera-outline';
      case 'editor': return 'brush-outline';
      case 'admin': return 'shield-outline';
      case 'viewer': return 'eye-outline';
      default: return 'person-outline';
    }
  };

  const getStatusColor = (status: Collaborator['status']) => {
    switch (status) {
      case 'online': return theme.colors.system.green;
      case 'away': return theme.colors.system.orange;
      case 'offline': return theme.colors.system.red;
      default: return theme.colors.gray;
    }
  };

  const formatLastSeen = (timestamp: string) => {
    const date = new Date(timestamp);
    const now = new Date();
    const diffInMinutes = Math.floor((now.getTime() - date.getTime()) / (1000 * 60));
    
    if (diffInMinutes < 1) return 'Just now';
    if (diffInMinutes < 60) return `${diffInMinutes}m ago`;
    if (diffInMinutes < 1440) return `${Math.floor(diffInMinutes / 60)}h ago`;
    return date.toLocaleDateString();
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
            <Text style={styles.title}>Team</Text>
            <View style={styles.onlineIndicator}>
              <View style={styles.onlineDot} />
              <Text style={styles.onlineText}>
                {collaborators.filter(c => c.status === 'online').length} online
              </Text>
            </View>
          </View>
          <TouchableOpacity onPress={onClose} style={styles.closeButton}>
            <Ionicons name="close" size={24} color={theme.colors.text.secondary} />
          </TouchableOpacity>
        </View>

        {/* Invite Section */}
        <View style={styles.inviteSection}>
          <TouchableOpacity 
            style={styles.inviteButton}
            onPress={() => setShowInviteForm(!showInviteForm)}
          >
            <Ionicons name="person-add-outline" size={20} color={theme.colors.green} />
            <Text style={styles.inviteButtonText}>Invite Collaborator</Text>
          </TouchableOpacity>
          
          <TouchableOpacity style={styles.qrButton} onPress={handleShowQR}>
            <Ionicons name="qr-code-outline" size={20} color={theme.colors.text.secondary} />
          </TouchableOpacity>
        </View>

        {/* Invite Form */}
        {showInviteForm && (
          <View style={styles.inviteForm}>
            <View style={styles.emailInput}>
              <Ionicons name="mail-outline" size={20} color={theme.colors.text.secondary} />
              <TextInput
                style={styles.input}
                placeholder="Enter email address"
                placeholderTextColor={theme.colors.text.tertiary}
                value={inviteEmail}
                onChangeText={setInviteEmail}
                keyboardType="email-address"
                autoCapitalize="none"
              />
            </View>
            <TouchableOpacity style={styles.sendInviteButton} onPress={handleInvite}>
              <Text style={styles.sendInviteText}>Send Invite</Text>
            </TouchableOpacity>
          </View>
        )}

        {/* Collaborators List */}
        <ScrollView style={styles.collaboratorsContainer} showsVerticalScrollIndicator={false}>
          {collaborators.map((collaborator) => (
            <TouchableOpacity
              key={collaborator.id}
              style={styles.collaboratorCard}
              onPress={() => handleCollaboratorPress(collaborator)}
              activeOpacity={0.8}
            >
              {/* Avatar */}
              <View style={styles.avatarContainer}>
                <View style={styles.avatar}>
                  <Text style={styles.avatarText}>{collaborator.avatar}</Text>
                </View>
                
                {/* Status Indicator */}
                <View style={[
                  styles.statusIndicator,
                  { backgroundColor: getStatusColor(collaborator.status) }
                ]} />
                
                {/* Active Pulse */}
                {collaborator.isActive && collaborator.status === 'online' && (
                  <Animated.View 
                    style={[
                      styles.activePulse,
                      { transform: [{ scale: pulseAnim }] }
                    ]} 
                  />
                )}
              </View>

              {/* Collaborator Info */}
              <View style={styles.collaboratorInfo}>
                <View style={styles.nameRow}>
                  <Text style={styles.collaboratorName} numberOfLines={1}>
                    {collaborator.name}
                  </Text>
                  
                  {/* Role Badge */}
                  <View style={[styles.roleBadge, { backgroundColor: getRoleColor(collaborator.role) + '20' }]}>
                    <Ionicons 
                      name={getRoleIcon(collaborator.role)} 
                      size={12} 
                      color={getRoleColor(collaborator.role)} 
                    />
                    <Text style={[styles.roleText, { color: getRoleColor(collaborator.role) }]}>
                      {collaborator.role}
                    </Text>
                  </View>
                </View>
                
                <Text style={styles.collaboratorEmail} numberOfLines={1}>
                  {collaborator.email}
                </Text>
                
                {/* Current Activity */}
                {collaborator.currentActivity && (
                  <View style={styles.activityRow}>
                    <Ionicons name="radio-outline" size={12} color={theme.colors.text.tertiary} />
                    <Text style={styles.activityText} numberOfLines={1}>
                      {collaborator.currentActivity}
                    </Text>
                  </View>
                )}
                
                <Text style={styles.lastSeenText}>
                  {collaborator.status === 'online' ? 'Online now' : `Last seen ${formatLastSeen(collaborator.lastSeen)}`}
                </Text>
              </View>

              {/* Actions */}
              <View style={styles.collaboratorActions}>
                <TouchableOpacity style={styles.actionButton}>
                  <Ionicons name="ellipsis-vertical" size={20} color={theme.colors.text.tertiary} />
                </TouchableOpacity>
              </View>
            </TouchableOpacity>
          ))}
        </ScrollView>

        {/* Role Management */}
        <View style={styles.roleManagement}>
          <Text style={styles.roleManagementTitle}>Quick Role Changes</Text>
          <ScrollView 
            horizontal 
            showsHorizontalScrollIndicator={false}
            contentContainerStyle={styles.roleOptions}
          >
            {(['photographer', 'editor', 'admin', 'viewer'] as const).map((role) => (
              <TouchableOpacity
                key={role}
                style={[styles.roleOption, { backgroundColor: getRoleColor(role) + '20' }]}
                onPress={() => {
                  if (selectedCollaborator) {
                    handleRoleChange(selectedCollaborator.id, role);
                  }
                }}
              >
                <Ionicons 
                  name={getRoleIcon(role)} 
                  size={16} 
                  color={getRoleColor(role)} 
                />
                <Text style={[styles.roleOptionText, { color: getRoleColor(role) }]}>
                  {role}
                </Text>
              </TouchableOpacity>
            ))}
          </ScrollView>
        </View>

        {/* QR Code Modal */}
        {showQRCode && (
          <View style={styles.qrOverlay}>
            <View style={styles.qrContainer}>
              <View style={styles.qrHeader}>
                <Text style={styles.qrTitle}>Invite via QR Code</Text>
                <TouchableOpacity onPress={() => setShowQRCode(false)}>
                  <Ionicons name="close" size={24} color={theme.colors.text.secondary} />
                </TouchableOpacity>
              </View>
              
              <View style={styles.qrContent}>
                <View style={styles.qrPlaceholder}>
                  <Ionicons name="qr-code" size={120} color={theme.colors.text.tertiary} />
                </View>
                <Text style={styles.qrInstructions}>
                  Scan this QR code to join the project
                </Text>
                <Text style={styles.qrProjectInfo}>
                  Project: Mountain Landscape Collection
                </Text>
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
    width: 360,
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
  onlineIndicator: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: theme.colors.system.green + '20',
    paddingHorizontal: theme.spacing.xs,
    paddingVertical: 2,
    borderRadius: theme.borderRadius.s,
    gap: 2,
  },
  onlineDot: {
    width: 6,
    height: 6,
    borderRadius: 3,
    backgroundColor: theme.colors.system.green,
  },
  onlineText: {
    fontSize: theme.fontSizes.caption,
    color: theme.colors.system.green,
    fontWeight: '600',
  },
  closeButton: {
    padding: theme.spacing.s,
  },
  inviteSection: {
    flexDirection: 'row',
    padding: theme.spacing.l,
    gap: theme.spacing.s,
    borderBottomWidth: 1,
    borderBottomColor: theme.colors.border,
  },
  inviteButton: {
    flex: 1,
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'center',
    paddingVertical: theme.spacing.m,
    paddingHorizontal: theme.spacing.m,
    backgroundColor: theme.colors.green + '10',
    borderRadius: theme.borderRadius.m,
    gap: theme.spacing.s,
  },
  inviteButtonText: {
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.green,
  },
  qrButton: {
    padding: theme.spacing.m,
    backgroundColor: theme.colors.lightGray,
    borderRadius: theme.borderRadius.m,
  },
  inviteForm: {
    flexDirection: 'row',
    padding: theme.spacing.l,
    gap: theme.spacing.s,
    borderBottomWidth: 1,
    borderBottomColor: theme.colors.border,
  },
  emailInput: {
    flex: 1,
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: theme.colors.lightGray,
    borderRadius: theme.borderRadius.m,
    paddingHorizontal: theme.spacing.m,
    paddingVertical: theme.spacing.s,
    gap: theme.spacing.s,
  },
  input: {
    flex: 1,
    fontSize: theme.fontSizes.body,
    color: theme.colors.text.primary,
  },
  sendInviteButton: {
    paddingVertical: theme.spacing.m,
    paddingHorizontal: theme.spacing.m,
    backgroundColor: theme.colors.green,
    borderRadius: theme.borderRadius.m,
    justifyContent: 'center',
  },
  sendInviteText: {
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.surface,
  },
  collaboratorsContainer: {
    flex: 1,
    paddingHorizontal: theme.spacing.l,
  },
  collaboratorCard: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingVertical: theme.spacing.m,
    paddingHorizontal: theme.spacing.m,
    backgroundColor: theme.colors.lightGray,
    borderRadius: theme.borderRadius.m,
    marginBottom: theme.spacing.s,
  },
  avatarContainer: {
    position: 'relative',
    marginRight: theme.spacing.m,
  },
  avatar: {
    width: 48,
    height: 48,
    borderRadius: 24,
    backgroundColor: theme.colors.green,
    justifyContent: 'center',
    alignItems: 'center',
  },
  avatarText: {
    fontSize: theme.fontSizes.headline,
    fontWeight: '700',
    color: theme.colors.surface,
  },
  statusIndicator: {
    position: 'absolute',
    bottom: 2,
    right: 2,
    width: 12,
    height: 12,
    borderRadius: 6,
    borderWidth: 2,
    borderColor: theme.colors.surface,
  },
  activePulse: {
    position: 'absolute',
    top: -2,
    left: -2,
    right: -2,
    bottom: -2,
    borderRadius: 26,
    backgroundColor: theme.colors.system.green + '30',
    borderWidth: 2,
    borderColor: theme.colors.system.green + '50',
  },
  collaboratorInfo: {
    flex: 1,
  },
  nameRow: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    marginBottom: theme.spacing.xs,
  },
  collaboratorName: {
    fontSize: theme.fontSizes.headline,
    fontWeight: '700',
    color: theme.colors.text.primary,
    flex: 1,
    marginRight: theme.spacing.s,
  },
  roleBadge: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: theme.spacing.xs,
    paddingVertical: 2,
    borderRadius: theme.borderRadius.s,
    gap: 2,
  },
  roleText: {
    fontSize: theme.fontSizes.caption,
    fontWeight: '600',
  },
  collaboratorEmail: {
    fontSize: theme.fontSizes.body,
    color: theme.colors.text.secondary,
    marginBottom: theme.spacing.xs,
  },
  activityRow: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: theme.spacing.xs,
    marginBottom: theme.spacing.xs,
  },
  activityText: {
    fontSize: theme.fontSizes.caption,
    color: theme.colors.text.secondary,
    fontWeight: '500',
    flex: 1,
  },
  lastSeenText: {
    fontSize: theme.fontSizes.caption,
    color: theme.colors.text.tertiary,
    fontWeight: '500',
  },
  collaboratorActions: {
    paddingLeft: theme.spacing.s,
  },
  actionButton: {
    padding: theme.spacing.s,
  },
  roleManagement: {
    padding: theme.spacing.l,
    borderTopWidth: 1,
    borderTopColor: theme.colors.border,
  },
  roleManagementTitle: {
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.text.primary,
    marginBottom: theme.spacing.m,
  },
  roleOptions: {
    gap: theme.spacing.s,
  },
  roleOption: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: theme.spacing.m,
    paddingVertical: theme.spacing.s,
    borderRadius: theme.borderRadius.m,
    gap: theme.spacing.xs,
  },
  roleOptionText: {
    fontSize: theme.fontSizes.caption,
    fontWeight: '600',
  },
  qrOverlay: {
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
  qrContainer: {
    backgroundColor: theme.colors.surface,
    borderRadius: theme.borderRadius.xl,
    width: 300,
    padding: theme.spacing.l,
    ...theme.shadows.large,
  },
  qrHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: theme.spacing.l,
  },
  qrTitle: {
    fontSize: theme.fontSizes.title,
    fontWeight: '700',
    color: theme.colors.text.primary,
  },
  qrContent: {
    alignItems: 'center',
    gap: theme.spacing.m,
  },
  qrPlaceholder: {
    width: 150,
    height: 150,
    backgroundColor: theme.colors.lightGray,
    borderRadius: theme.borderRadius.m,
    justifyContent: 'center',
    alignItems: 'center',
  },
  qrInstructions: {
    fontSize: theme.fontSizes.body,
    color: theme.colors.text.primary,
    fontWeight: '600',
    textAlign: 'center',
  },
  qrProjectInfo: {
    fontSize: theme.fontSizes.caption,
    color: theme.colors.text.secondary,
    textAlign: 'center',
  },
});

export default CollaboratorAvatars;
