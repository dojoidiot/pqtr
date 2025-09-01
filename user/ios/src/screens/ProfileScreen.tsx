import React, { useRef } from 'react';
import {
  SafeAreaView,
  Text,
  View,
  StyleSheet,
  ScrollView,
  TouchableOpacity,
  Switch,
  Alert,
  Animated,
  Image,
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import { theme } from '../constants/theme';
import { useUserProfile } from '../hooks/useUserProfile';

const ProfileScreen: React.FC = () => {
  const {
    userProfile,
    handleEditProfile,
    handleThemeToggle,
    handleNotificationsToggle,
    handleAutoPresetsToggle,
    handleExportFormatChange,
    handleManageCollaborators,
    handleManageBilling,
    getStoragePercentage,
    getStorageColor,
    formatLastActive,
  } = useUserProfile();
  
  const fadeAnim = useRef(new Animated.Value(0)).current;
  const slideAnim = useRef(new Animated.Value(20)).current;

  React.useEffect(() => {
    Animated.parallel([
      Animated.timing(fadeAnim, {
        toValue: 1,
        duration: 600,
        useNativeDriver: true,
      }),
      Animated.timing(slideAnim, {
        toValue: 0,
        duration: 600,
        useNativeDriver: true,
      }),
    ]).start();
  }, []);

  const handleLogout = () => {
    Alert.alert(
      'Logout',
      'Are you sure you want to logout?',
      [
        { text: 'Cancel', style: 'cancel' },
        { text: 'Logout', style: 'destructive' },
      ]
    );
  };

  const handleDeleteAccount = () => {
    Alert.alert(
      'Delete Account',
      'This action cannot be undone. All your data will be permanently deleted.',
      [
        { text: 'Cancel', style: 'cancel' },
        { text: 'Delete', style: 'destructive', onPress: () => {
          Alert.alert(
            'Final Confirmation',
            'Type "DELETE" to confirm account deletion',
            [
              { text: 'Cancel', style: 'cancel' },
              { text: 'Delete', style: 'destructive' },
            ]
          );
        }},
      ]
    );
  };

  return (
    <SafeAreaView style={styles.container}>
      <Animated.View 
        style={[
          styles.content,
          {
            opacity: fadeAnim,
            transform: [{ translateY: slideAnim }],
          }
        ]}
      >
        {/* Header Section */}
        <View style={styles.header}>
          <View style={styles.avatarContainer}>
            <Image source={{ uri: userProfile.avatar }} style={styles.avatar} />
            <TouchableOpacity style={styles.editAvatarButton}>
              <Ionicons name="camera" size={16} color={theme.colors.surface} />
            </TouchableOpacity>
          </View>
          
          <Text style={styles.fullName}>{userProfile.fullName}</Text>
          <Text style={styles.email}>{userProfile.email}</Text>
          <View style={styles.roleContainer}>
            <View style={[styles.roleBadge, { backgroundColor: theme.colors.system.blue }]}>
              <Ionicons name="shield-checkmark" size={14} color={theme.colors.surface} />
              <Text style={styles.roleText}>{userProfile.role}</Text>
            </View>
          </View>
          
          <TouchableOpacity style={styles.editProfileButton} onPress={handleEditProfile}>
            <Ionicons name="create-outline" size={16} color={theme.colors.system.blue} />
            <Text style={styles.editProfileText}>Edit Profile</Text>
          </TouchableOpacity>
        </View>

        <ScrollView style={styles.scrollContent} showsVerticalScrollIndicator={false}>
          {/* Account Stats */}
          <View style={styles.section}>
            <Text style={styles.sectionTitle}>Account Overview</Text>
            <ScrollView horizontal showsHorizontalScrollIndicator={false} style={styles.statsContainer}>
              <View style={styles.statCard}>
                <Ionicons name="folder-outline" size={24} color={theme.colors.system.blue} />
                <Text style={styles.statNumber}>{userProfile.totalProjects}</Text>
                <Text style={styles.statLabel}>Projects</Text>
              </View>
              
              <View style={styles.statCard}>
                <Ionicons name="images-outline" size={24} color={theme.colors.system.green} />
                <Text style={styles.statNumber}>{userProfile.totalPhotos.toLocaleString()}</Text>
                <Text style={styles.statLabel}>Photos</Text>
              </View>
              
              <View style={styles.statCard}>
                <Ionicons name="sync-outline" size={24} color={theme.colors.system.orange} />
                <Text style={styles.statNumber}>{userProfile.syncsLast24h}</Text>
                <Text style={styles.statLabel}>Syncs (24h)</Text>
              </View>
              
              <View style={styles.statCard}>
                <Ionicons name="people-outline" size={24} color={theme.colors.system.purple} />
                <Text style={styles.statNumber}>{userProfile.collaborators}</Text>
                <Text style={styles.statLabel}>Collaborators</Text>
              </View>
            </ScrollView>
          </View>

          {/* Storage Section */}
          <View style={styles.section}>
            <View style={styles.storageHeader}>
              <Text style={styles.sectionTitle}>Storage</Text>
              <Text style={styles.storageText}>
                {userProfile.storageUsed.toFixed(1)}GB / {userProfile.storageTotal}GB
              </Text>
            </View>
            <View style={styles.storageBar}>
              <View 
                style={[
                  styles.storageProgress, 
                  { 
                    width: `${getStoragePercentage()}%`,
                    backgroundColor: getStorageColor(),
                  }
                ]} 
              />
            </View>
            <Text style={styles.storagePercentage}>{getStoragePercentage().toFixed(0)}% used</Text>
          </View>

          {/* Preferences */}
          <View style={styles.section}>
            <Text style={styles.sectionTitle}>Preferences</Text>
            
            <View style={styles.preferenceItem}>
              <View style={styles.preferenceLeft}>
                <Ionicons name="moon-outline" size={20} color={theme.colors.text.secondary} />
                <Text style={styles.preferenceLabel}>Dark Theme</Text>
              </View>
              <Switch
                value={userProfile.preferences.theme === 'dark'}
                onValueChange={handleThemeToggle}
                trackColor={{ false: theme.colors.lightGray, true: theme.colors.system.blue }}
                thumbColor={theme.colors.surface}
              />
            </View>

            <View style={styles.preferenceItem}>
              <View style={styles.preferenceLeft}>
                <Ionicons name="notifications-outline" size={20} color={theme.colors.text.secondary} />
                <Text style={styles.preferenceLabel}>Notifications</Text>
              </View>
              <Switch
                value={userProfile.preferences.notifications}
                onValueChange={handleNotificationsToggle}
                trackColor={{ false: theme.colors.lightGray, true: theme.colors.system.blue }}
                thumbColor={theme.colors.surface}
              />
            </View>

            <View style={styles.preferenceItem}>
              <View style={styles.preferenceLeft}>
                <Ionicons name="download-outline" size={20} color={theme.colors.text.secondary} />
                <Text style={styles.preferenceLabel}>Default Export Format</Text>
              </View>
              <View style={styles.formatSelector}>
                {(['JPEG', 'PNG', 'RAW'] as const).map((format) => (
                  <TouchableOpacity
                    key={format}
                    style={[
                      styles.formatOption,
                      userProfile.preferences.defaultExportFormat === format && styles.formatOptionActive
                    ]}
                    onPress={() => handleExportFormatChange(format)}
                  >
                    <Text style={[
                      styles.formatOptionText,
                      userProfile.preferences.defaultExportFormat === format && styles.formatOptionTextActive
                    ]}>
                      {format}
                    </Text>
                  </TouchableOpacity>
                ))}
              </View>
            </View>

            <View style={styles.preferenceItem}>
              <View style={styles.preferenceLeft}>
                <Ionicons name="sparkles-outline" size={20} color={theme.colors.text.secondary} />
                <Text style={styles.preferenceLabel}>Auto-apply Presets</Text>
              </View>
              <Switch
                value={userProfile.preferences.autoApplyPresets}
                onValueChange={handleAutoPresetsToggle}
                trackColor={{ false: theme.colors.lightGray, true: theme.colors.system.blue }}
                thumbColor={theme.colors.surface}
              />
            </View>
          </View>

          {/* Collaboration Settings */}
          <View style={styles.section}>
            <Text style={styles.sectionTitle}>Collaboration</Text>
            
            <TouchableOpacity style={styles.collaborationItem} onPress={handleManageCollaborators}>
              <View style={styles.collaborationLeft}>
                <Ionicons name="people-outline" size={20} color={theme.colors.text.secondary} />
                <Text style={styles.collaborationLabel}>Manage Collaborators</Text>
              </View>
              <Ionicons name="chevron-forward" size={20} color={theme.colors.text.tertiary} />
            </TouchableOpacity>

            <View style={styles.collaborationItem}>
              <View style={styles.collaborationLeft}>
                <Ionicons name="shield-outline" size={20} color={theme.colors.text.secondary} />
                <Text style={styles.collaborationLabel}>Current Role</Text>
              </View>
              <View style={[styles.roleBadge, { backgroundColor: theme.colors.system.blue }]}>
                <Text style={styles.roleText}>{userProfile.role}</Text>
              </View>
            </View>

            <View style={styles.collaborationItem}>
              <View style={styles.collaborationLeft}>
                <Ionicons name="share-outline" size={20} color={theme.colors.text.secondary} />
                <Text style={styles.collaborationLabel}>Project Sharing</Text>
              </View>
              <Text style={styles.collaborationValue}>Enabled</Text>
            </View>
          </View>

          {/* Account Management */}
          <View style={styles.section}>
            <Text style={styles.sectionTitle}>Account</Text>
            
            <View style={styles.accountItem}>
              <View style={styles.accountLeft}>
                <Ionicons name="diamond-outline" size={20} color={theme.colors.system.purple} />
                <Text style={styles.accountLabel}>Subscription</Text>
              </View>
              <View style={[styles.subscriptionBadge, { backgroundColor: theme.colors.system.purple }]}>
                <Text style={styles.subscriptionText}>{userProfile.subscriptionTier}</Text>
              </View>
            </View>

            <TouchableOpacity style={styles.accountItem} onPress={handleManageBilling}>
              <View style={styles.accountLeft}>
                <Ionicons name="card-outline" size={20} color={theme.colors.text.secondary} />
                <Text style={styles.accountLabel}>Manage Billing</Text>
              </View>
              <Ionicons name="chevron-forward" size={20} color={theme.colors.text.tertiary} />
            </TouchableOpacity>

            <View style={styles.accountItem}>
              <View style={styles.accountLeft}>
                <Ionicons name="time-outline" size={20} color={theme.colors.text.secondary} />
                <Text style={styles.accountLabel}>Last Active</Text>
              </View>
              <Text style={styles.accountValue}>{formatLastActive(userProfile.lastActive)}</Text>
            </View>
          </View>

          {/* Action Buttons */}
          <View style={styles.section}>
            <TouchableOpacity style={styles.logoutButton} onPress={handleLogout}>
              <Ionicons name="log-out-outline" size={20} color={theme.colors.system.blue} />
              <Text style={styles.logoutButtonText}>Logout</Text>
            </TouchableOpacity>

            <TouchableOpacity style={styles.deleteButton} onPress={handleDeleteAccount}>
              <Ionicons name="trash-outline" size={20} color={theme.colors.status.error} />
              <Text style={styles.deleteButtonText}>Delete Account</Text>
            </TouchableOpacity>
          </View>

          <View style={styles.bottomSpacing} />
        </ScrollView>
      </Animated.View>
    </SafeAreaView>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: theme.colors.background,
  },
  content: {
    flex: 1,
  },
  header: {
    alignItems: 'center',
    paddingVertical: theme.spacing.xl,
    paddingHorizontal: theme.spacing.m,
    backgroundColor: theme.colors.surface,
    borderBottomLeftRadius: theme.borderRadius.xl,
    borderBottomRightRadius: theme.borderRadius.xl,
    ...theme.shadows.medium,
  },
  avatarContainer: {
    position: 'relative',
    marginBottom: theme.spacing.m,
  },
  avatar: {
    width: 100,
    height: 100,
    borderRadius: 50,
    borderWidth: 4,
    borderColor: theme.colors.surface,
    ...theme.shadows.large,
  },
  editAvatarButton: {
    position: 'absolute',
    bottom: 0,
    right: 0,
    backgroundColor: theme.colors.system.blue,
    width: 32,
    height: 32,
    borderRadius: 16,
    justifyContent: 'center',
    alignItems: 'center',
    borderWidth: 3,
    borderColor: theme.colors.surface,
  },
  fullName: {
    fontSize: theme.fontSizes.ios.title1,
    fontWeight: '700',
    color: theme.colors.text.primary,
    marginBottom: theme.spacing.xs,
  },
  email: {
    fontSize: theme.fontSizes.ios.body,
    color: theme.colors.text.secondary,
    marginBottom: theme.spacing.m,
  },
  roleContainer: {
    marginBottom: theme.spacing.m,
  },
  roleBadge: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: theme.spacing.m,
    paddingVertical: theme.spacing.xs,
    borderRadius: theme.borderRadius.l,
  },
  roleText: {
    color: theme.colors.surface,
    fontSize: theme.fontSizes.ios.footnote,
    fontWeight: '600',
    marginLeft: theme.spacing.xs,
  },
  editProfileButton: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: theme.spacing.m,
    paddingVertical: theme.spacing.s,
    borderRadius: theme.borderRadius.m,
    backgroundColor: theme.colors.lightGray,
  },
  editProfileText: {
    color: theme.colors.system.blue,
    fontSize: theme.fontSizes.ios.callout,
    fontWeight: '600',
    marginLeft: theme.spacing.xs,
  },
  scrollContent: {
    flex: 1,
    paddingHorizontal: theme.spacing.m,
  },
  section: {
    marginTop: theme.spacing.xl,
  },
  sectionTitle: {
    fontSize: theme.fontSizes.ios.headline,
    fontWeight: '600',
    color: theme.colors.text.primary,
    marginBottom: theme.spacing.m,
  },
  statsContainer: {
    marginHorizontal: -theme.spacing.xs,
  },
  statCard: {
    backgroundColor: theme.colors.surface,
    padding: theme.spacing.m,
    borderRadius: theme.borderRadius.l,
    alignItems: 'center',
    marginHorizontal: theme.spacing.xs,
    minWidth: 80,
    ...theme.shadows.subtle,
  },
  statNumber: {
    fontSize: theme.fontSizes.ios.title2,
    fontWeight: '700',
    color: theme.colors.text.primary,
    marginTop: theme.spacing.xs,
  },
  statLabel: {
    fontSize: theme.fontSizes.ios.footnote,
    color: theme.colors.text.secondary,
    marginTop: theme.spacing.xs,
    textAlign: 'center',
  },
  storageHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: theme.spacing.s,
  },
  storageText: {
    fontSize: theme.fontSizes.ios.footnote,
    color: theme.colors.text.secondary,
  },
  storageBar: {
    height: 8,
    backgroundColor: theme.colors.lightGray,
    borderRadius: 4,
    overflow: 'hidden',
    marginBottom: theme.spacing.xs,
  },
  storageProgress: {
    height: '100%',
    borderRadius: 4,
  },
  storagePercentage: {
    fontSize: theme.fontSizes.ios.footnote,
    color: theme.colors.text.secondary,
    textAlign: 'right',
  },
  preferenceItem: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingVertical: theme.spacing.m,
    borderBottomWidth: 1,
    borderBottomColor: theme.colors.border,
  },
  preferenceLeft: {
    flexDirection: 'row',
    alignItems: 'center',
    flex: 1,
  },
  preferenceLabel: {
    fontSize: theme.fontSizes.ios.body,
    color: theme.colors.text.primary,
    marginLeft: theme.spacing.m,
    flex: 1,
  },
  formatSelector: {
    flexDirection: 'row',
    gap: theme.spacing.xs,
  },
  formatOption: {
    paddingHorizontal: theme.spacing.m,
    paddingVertical: theme.spacing.xs,
    borderRadius: theme.borderRadius.m,
    backgroundColor: theme.colors.lightGray,
  },
  formatOptionActive: {
    backgroundColor: theme.colors.system.blue,
  },
  formatOptionText: {
    fontSize: theme.fontSizes.ios.footnote,
    color: theme.colors.text.secondary,
    fontWeight: '500',
  },
  formatOptionTextActive: {
    color: theme.colors.surface,
  },
  collaborationItem: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingVertical: theme.spacing.m,
    borderBottomWidth: 1,
    borderBottomColor: theme.colors.border,
  },
  collaborationLeft: {
    flexDirection: 'row',
    alignItems: 'center',
    flex: 1,
  },
  collaborationLabel: {
    fontSize: theme.fontSizes.ios.body,
    color: theme.colors.text.primary,
    marginLeft: theme.spacing.m,
    flex: 1,
  },
  collaborationValue: {
    fontSize: theme.fontSizes.ios.body,
    color: theme.colors.text.secondary,
  },
  accountItem: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingVertical: theme.spacing.m,
    borderBottomWidth: 1,
    borderBottomColor: theme.colors.border,
  },
  accountLeft: {
    flexDirection: 'row',
    alignItems: 'center',
    flex: 1,
  },
  accountLabel: {
    fontSize: theme.fontSizes.ios.body,
    color: theme.colors.text.primary,
    marginLeft: theme.spacing.m,
    flex: 1,
  },
  accountValue: {
    fontSize: theme.fontSizes.ios.body,
    color: theme.colors.text.secondary,
  },
  subscriptionBadge: {
    paddingHorizontal: theme.spacing.m,
    paddingVertical: theme.spacing.xs,
    borderRadius: theme.borderRadius.m,
  },
  subscriptionText: {
    color: theme.colors.surface,
    fontSize: theme.fontSizes.ios.footnote,
    fontWeight: '600',
  },
  logoutButton: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'center',
    paddingVertical: theme.spacing.m,
    paddingHorizontal: theme.spacing.l,
    borderRadius: theme.borderRadius.m,
    backgroundColor: theme.colors.lightGray,
    marginBottom: theme.spacing.m,
  },
  logoutButtonText: {
    color: theme.colors.system.blue,
    fontSize: theme.fontSizes.ios.body,
    fontWeight: '600',
    marginLeft: theme.spacing.s,
  },
  deleteButton: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'center',
    paddingVertical: theme.spacing.m,
    paddingHorizontal: theme.spacing.l,
    borderRadius: theme.borderRadius.m,
    backgroundColor: theme.colors.lightGray,
  },
  deleteButtonText: {
    color: theme.colors.status.error,
    fontSize: theme.fontSizes.ios.body,
    fontWeight: '600',
    marginLeft: theme.spacing.s,
  },
  bottomSpacing: {
    height: theme.spacing.xxl,
  },
});

export default ProfileScreen; 