import React, { useState, useRef } from 'react';
import {
  SafeAreaView,
  Text,
  View,
  StyleSheet,
  ScrollView,
  TextInput,
  TouchableOpacity,
  Switch,
  FlatList,
  Alert,
  Animated,
  Dimensions,
  Modal,
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import * as Haptics from 'expo-haptics';
import { theme } from '../constants/theme';

const { width: screenWidth } = Dimensions.get('window');

interface Tag {
  id: string;
  label: string;
  selected: boolean;
}

interface Preset {
  id: string;
  name: string;
  description: string;
  thumbnailUri: string;
}

interface Collaborator {
  id: string;
  email: string;
}

const mockTags: Tag[] = [
  { id: '1', label: 'Formula 1', selected: false },
  { id: '2', label: 'Press', selected: false },
  { id: '3', label: 'VIP', selected: false },
  { id: '4', label: 'Event', selected: false },
  { id: '5', label: 'Portrait', selected: false },
  { id: '6', label: 'Landscape', selected: false },
  { id: '7', label: 'Street', selected: false },
  { id: '8', label: 'Studio', selected: false },
];

const mockPresets: Preset[] = [
  {
    id: '1',
    name: 'Vivid Landscape',
    description: 'Enhanced colors for outdoor photography',
    thumbnailUri: 'https://images.unsplash.com/photo-1506905925346-21bda4d32df4?w=100&h=100&fit=crop',
  },
  {
    id: '2',
    name: 'Moody Portrait',
    description: 'Dramatic lighting for people shots',
    thumbnailUri: 'https://images.unsplash.com/photo-1472099645785-5658abf4ff4e?w=100&h=100&fit=crop',
  },
  {
    id: '3',
    name: 'Urban Contrast',
    description: 'High contrast for city photography',
    thumbnailUri: 'https://images.unsplash.com/photo-1441974231531-c6227db76b6e?w=100&h=100&fit=crop',
  },
  {
    id: '4',
    name: 'Natural Light',
    description: 'Soft, natural lighting enhancement',
    thumbnailUri: 'https://images.unsplash.com/photo-1506905925346-21bda4d32df4?w=100&h=100&fit=crop',
  },
];

const AddProjectScreen: React.FC = () => {
  const [projectTitle, setProjectTitle] = useState('');
  const [projectDate, setProjectDate] = useState(new Date());
  const [location, setLocation] = useState('');
  const [tags, setTags] = useState<Tag[]>(mockTags);
  const [selectedPreset, setSelectedPreset] = useState<string | null>(null);
  const [cameraConnection, setCameraConnection] = useState(false);
  const [autoUpload, setAutoUpload] = useState(false);
  const [collaborators, setCollaborators] = useState<Collaborator[]>([]);
  const [newCollaboratorEmail, setNewCollaboratorEmail] = useState('');
  const [showDatePicker, setShowDatePicker] = useState(false);
  const [tempDate, setTempDate] = useState(new Date());

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

  const handleTagToggle = (tagId: string) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    setTags(prev => prev.map(tag => 
      tag.id === tagId ? { ...tag, selected: !tag.selected } : tag
    ));
  };

  const handlePresetSelect = (presetId: string) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    setSelectedPreset(selectedPreset === presetId ? null : presetId);
  };

  const handleAddCollaborator = () => {
    if (!newCollaboratorEmail.trim()) return;
    
    const emailRegex = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
    if (!emailRegex.test(newCollaboratorEmail)) {
      Alert.alert('Invalid Email', 'Please enter a valid email address.');
      return;
    }

    if (collaborators.some(c => c.email.toLowerCase() === newCollaboratorEmail.toLowerCase())) {
      Alert.alert('Duplicate Email', 'This collaborator has already been added.');
      return;
    }

    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    const newCollaborator: Collaborator = {
      id: Date.now().toString(),
      email: newCollaboratorEmail.trim(),
    };
    
    setCollaborators(prev => [...prev, newCollaborator]);
    setNewCollaboratorEmail('');
  };

  const handleRemoveCollaborator = (collaboratorId: string) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    setCollaborators(prev => prev.filter(c => c.id !== collaboratorId));
  };

  const handleDateChange = (event: any, selectedDate?: Date) => {
    if (selectedDate) {
      setTempDate(selectedDate);
    }
  };

  const handleDateConfirm = () => {
    setProjectDate(tempDate);
    setShowDatePicker(false);
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
  };

  const handleDateCancel = () => {
    setTempDate(projectDate);
    setShowDatePicker(false);
  };

  const handleCreateProject = () => {
    if (!projectTitle.trim()) {
      Alert.alert('Missing Title', 'Please enter a project title.');
      return;
    }

    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Medium);
    
    const selectedTags = tags.filter(tag => tag.selected);
    const projectData = {
      title: projectTitle.trim(),
      date: projectDate,
      location: location.trim(),
      tags: selectedTags.map(tag => tag.label),
      preset: selectedPreset,
      cameraConnection,
      autoUpload,
      collaborators: collaborators.map(c => c.email),
    };

    console.log('Creating project:', projectData);
    Alert.alert('Success', 'Project created successfully!', [
      { text: 'OK', onPress: () => {
        // TODO: Navigate back or to project screen
      }}
    ]);
  };

  const formatDate = (date: Date) => {
    return date.toLocaleDateString('en-US', {
      weekday: 'short',
      year: 'numeric',
      month: 'short',
      day: 'numeric',
    });
  };

  const renderTag = ({ item }: { item: Tag }) => (
    <TouchableOpacity
      style={[
        styles.tagChip,
        item.selected && styles.tagChipSelected
      ]}
      onPress={() => handleTagToggle(item.id)}
      accessibilityLabel={`Tag: ${item.label}`}
      accessibilityRole="button"
      accessibilityState={{ selected: item.selected }}
    >
      <Text style={[
        styles.tagChipText,
        item.selected && styles.tagChipTextSelected
      ]}>
        {item.label}
      </Text>
    </TouchableOpacity>
  );

  const renderPreset = ({ item }: { item: Preset }) => (
    <TouchableOpacity
      style={[
        styles.presetCard,
        selectedPreset === item.id && styles.presetCardSelected
      ]}
      onPress={() => handlePresetSelect(item.id)}
      accessibilityLabel={`Preset: ${item.name}`}
      accessibilityRole="button"
      accessibilityState={{ selected: selectedPreset === item.id }}
    >
      <View style={styles.presetThumbnail}>
        <Ionicons 
          name="image-outline" 
          size={24} 
          color={selectedPreset === item.id ? theme.colors.surface : theme.colors.text.secondary} 
        />
      </View>
      <View style={styles.presetInfo}>
        <Text style={[
          styles.presetName,
          selectedPreset === item.id && styles.presetNameSelected
        ]}>
          {item.name}
        </Text>
        <Text style={[
          styles.presetDescription,
          selectedPreset === item.id && styles.presetDescriptionSelected
        ]}>
          {item.description}
        </Text>
      </View>
      {selectedPreset === item.id && (
        <View style={styles.presetCheckmark}>
          <Ionicons name="checkmark-circle" size={20} color={theme.colors.surface} />
        </View>
      )}
    </TouchableOpacity>
  );

  const renderCollaborator = ({ item }: { item: Collaborator }) => (
    <View style={styles.collaboratorChip}>
      <Text style={styles.collaboratorEmail}>{item.email}</Text>
      <TouchableOpacity
        style={styles.removeCollaboratorButton}
        onPress={() => handleRemoveCollaborator(item.id)}
        accessibilityLabel={`Remove collaborator ${item.email}`}
        accessibilityRole="button"
      >
        <Ionicons name="close-circle" size={16} color={theme.colors.status.error} />
      </TouchableOpacity>
    </View>
  );

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
        {/* Header */}
        <View style={styles.header}>
          <Text style={styles.headerTitle}>New Project</Text>
          <Text style={styles.headerSubtitle}>Create a new photography project</Text>
        </View>

        <ScrollView style={styles.scrollContent} showsVerticalScrollIndicator={false}>
          {/* Project Title */}
          <View style={styles.section}>
            <Text style={styles.sectionTitle}>Project Title</Text>
            <TextInput
              style={styles.textInput}
              placeholder="Enter project title"
              placeholderTextColor={theme.colors.text.tertiary}
              value={projectTitle}
              onChangeText={setProjectTitle}
              accessibilityLabel="Project title input"
              accessibilityHint="Enter the name for your new project"
            />
          </View>

          {/* Date Picker */}
          <View style={styles.section}>
            <Text style={styles.sectionTitle}>Project Date</Text>
            <TouchableOpacity
              style={styles.datePickerButton}
              onPress={() => setShowDatePicker(true)}
              accessibilityLabel="Select project date"
              accessibilityRole="button"
            >
              <Ionicons name="calendar-outline" size={20} color={theme.colors.text.secondary} />
              <Text style={styles.datePickerText}>{formatDate(projectDate)}</Text>
              <Ionicons name="chevron-forward" size={16} color={theme.colors.text.tertiary} />
            </TouchableOpacity>
          </View>

          {/* Location */}
          <View style={styles.section}>
            <Text style={styles.sectionTitle}>Location</Text>
            <TextInput
              style={styles.textInput}
              placeholder="Enter project location"
              placeholderTextColor={theme.colors.text.tertiary}
              value={location}
              onChangeText={setLocation}
              accessibilityLabel="Project location input"
              accessibilityHint="Enter the location where the project will take place"
            />
          </View>

          {/* Tags */}
          <View style={styles.section}>
            <Text style={styles.sectionTitle}>Tags</Text>
            <FlatList
              data={tags}
              renderItem={renderTag}
              keyExtractor={(item) => item.id}
              horizontal
              showsHorizontalScrollIndicator={false}
              contentContainerStyle={styles.tagsContainer}
            />
          </View>

          {/* Preset Picker */}
          <View style={styles.section}>
            <Text style={styles.sectionTitle}>Default Preset</Text>
            <Text style={styles.sectionSubtitle}>Choose a preset to apply to all images</Text>
            <FlatList
              data={mockPresets}
              renderItem={renderPreset}
              keyExtractor={(item) => item.id}
              horizontal
              showsHorizontalScrollIndicator={false}
              contentContainerStyle={styles.presetsContainer}
            />
          </View>

          {/* Camera Connection Toggle */}
          <View style={styles.section}>
            <View style={styles.toggleRow}>
              <View style={styles.toggleLeft}>
                <Ionicons name="camera-outline" size={20} color={theme.colors.text.secondary} />
                <View style={styles.toggleTextContainer}>
                  <Text style={styles.toggleTitle}>Camera Connection</Text>
                  <Text style={styles.toggleSubtitle}>Enable real-time ingestion</Text>
                </View>
              </View>
              <Switch
                value={cameraConnection}
                onValueChange={setCameraConnection}
                trackColor={{ false: theme.colors.lightGray, true: theme.colors.green }}
                thumbColor={theme.colors.surface}
                accessibilityLabel="Camera connection toggle"
                accessibilityHint="Enable or disable real-time camera ingestion"
              />
            </View>
          </View>

          {/* Auto Upload Toggle */}
          <View style={styles.section}>
            <View style={styles.toggleRow}>
              <View style={styles.toggleLeft}>
                <Ionicons name="cloud-upload-outline" size={20} color={theme.colors.text.secondary} />
                <View style={styles.toggleTextContainer}>
                  <Text style={styles.toggleTitle}>Auto Upload</Text>
                  <Text style={styles.toggleSubtitle}>Automatically upload when ingesting</Text>
                </View>
              </View>
              <Switch
                value={autoUpload}
                onValueChange={setAutoUpload}
                trackColor={{ false: theme.colors.lightGray, true: theme.colors.green }}
                thumbColor={theme.colors.surface}
                accessibilityLabel="Auto upload toggle"
                accessibilityHint="Enable or disable automatic upload during ingestion"
              />
            </View>
          </View>

          {/* Collaborators */}
          <View style={styles.section}>
            <Text style={styles.sectionTitle}>Collaborators</Text>
            <Text style={styles.sectionSubtitle}>Add team members to collaborate</Text>
            
            <View style={styles.collaboratorInputRow}>
              <TextInput
                style={styles.collaboratorInput}
                placeholder="Enter email address"
                placeholderTextColor={theme.colors.text.tertiary}
                value={newCollaboratorEmail}
                onChangeText={setNewCollaboratorEmail}
                keyboardType="email-address"
                autoCapitalize="none"
                accessibilityLabel="Collaborator email input"
                accessibilityHint="Enter email address to add a collaborator"
              />
              <TouchableOpacity
                style={styles.addCollaboratorButton}
                onPress={handleAddCollaborator}
                accessibilityLabel="Add collaborator"
                accessibilityRole="button"
              >
                <Ionicons name="add" size={20} color={theme.colors.surface} />
              </TouchableOpacity>
            </View>

            {collaborators.length > 0 && (
              <FlatList
                data={collaborators}
                renderItem={renderCollaborator}
                keyExtractor={(item) => item.id}
                horizontal
                showsHorizontalScrollIndicator={false}
                contentContainerStyle={styles.collaboratorsContainer}
              />
            )}
          </View>

          {/* Create Button */}
          <View style={styles.section}>
            <TouchableOpacity
              style={styles.createButton}
              onPress={handleCreateProject}
              accessibilityLabel="Create project"
              accessibilityRole="button"
              accessibilityHint="Creates a new project with the entered details"
            >
              <Ionicons name="checkmark" size={20} color={theme.colors.surface} />
              <Text style={styles.createButtonText}>Create Project</Text>
            </TouchableOpacity>
          </View>

          <View style={styles.bottomSpacing} />
        </ScrollView>
      </Animated.View>

      {/* Date Picker Modal */}
      <Modal
        visible={showDatePicker}
        transparent={true}
        animationType="slide"
        onRequestClose={handleDateCancel}
      >
        <View style={styles.modalOverlay}>
          <View style={styles.datePickerModal}>
            <View style={styles.datePickerHeader}>
              <TouchableOpacity onPress={handleDateCancel} style={styles.datePickerButton}>
                <Text style={styles.datePickerButtonText}>Cancel</Text>
              </TouchableOpacity>
              <Text style={styles.datePickerTitle}>Select Date</Text>
              <TouchableOpacity onPress={handleDateConfirm} style={styles.datePickerButton}>
                <Text style={[styles.datePickerButtonText, styles.datePickerButtonTextConfirm]}>Done</Text>
              </TouchableOpacity>
            </View>
            
            <View style={styles.datePickerContent}>
              <Text style={styles.datePickerLabel}>Current Date: {formatDate(tempDate)}</Text>
              <TouchableOpacity
                style={styles.datePickerInput}
                onPress={() => {
                  // For Expo Go compatibility, we'll use a simple date input
                  // In a real app, you'd integrate with @react-native-community/datetimepicker
                  Alert.alert('Date Picker', 'Date picker would open here. For now, the current date is selected.');
                }}
              >
                <Ionicons name="calendar" size={20} color={theme.colors.text.secondary} />
                <Text style={styles.datePickerInputText}>Tap to change date</Text>
              </TouchableOpacity>
            </View>
          </View>
        </View>
      </Modal>
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
    paddingTop: theme.spacing.l,
    paddingBottom: theme.spacing.m,
    paddingHorizontal: theme.spacing.l,
  },
  headerTitle: {
    fontSize: theme.fontSizes.largeTitle,
    fontWeight: '700',
    color: theme.colors.text.primary,
    letterSpacing: -0.5,
    marginBottom: theme.spacing.xs,
  },
  headerSubtitle: {
    fontSize: theme.fontSizes.body,
    color: theme.colors.text.secondary,
    fontWeight: '400',
  },
  scrollContent: {
    flex: 1,
    paddingHorizontal: theme.spacing.l,
  },
  section: {
    marginBottom: theme.spacing.xl,
  },
  sectionTitle: {
    fontSize: theme.fontSizes.headline,
    fontWeight: '600',
    color: theme.colors.text.primary,
    marginBottom: theme.spacing.m,
  },
  sectionSubtitle: {
    fontSize: theme.fontSizes.footnote,
    color: theme.colors.text.secondary,
    marginBottom: theme.spacing.m,
    fontStyle: 'italic',
  },
  textInput: {
    backgroundColor: theme.colors.surface,
    borderRadius: theme.borderRadius.m,
    paddingHorizontal: theme.spacing.m,
    paddingVertical: theme.spacing.m,
    fontSize: theme.fontSizes.body,
    color: theme.colors.text.primary,
    borderWidth: 1,
    borderColor: theme.colors.border,
    ...theme.shadows.subtle,
  },
  datePickerButton: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: theme.colors.surface,
    borderRadius: theme.borderRadius.m,
    paddingHorizontal: theme.spacing.m,
    paddingVertical: theme.spacing.m,
    borderWidth: 1,
    borderColor: theme.colors.border,
    ...theme.shadows.subtle,
  },
  datePickerText: {
    flex: 1,
    fontSize: theme.fontSizes.body,
    color: theme.colors.text.primary,
    marginLeft: theme.spacing.m,
  },
  tagsContainer: {
    paddingHorizontal: theme.spacing.xs,
  },
  tagChip: {
    paddingHorizontal: theme.spacing.m,
    paddingVertical: theme.spacing.s,
    borderRadius: theme.borderRadius.l,
    backgroundColor: theme.colors.lightGray,
    marginHorizontal: theme.spacing.xs,
    borderWidth: 1,
    borderColor: theme.colors.border,
  },
  tagChipSelected: {
    backgroundColor: theme.colors.green,
    borderColor: theme.colors.green,
  },
  tagChipText: {
    fontSize: theme.fontSizes.footnote,
    color: theme.colors.text.secondary,
    fontWeight: '500',
  },
  tagChipTextSelected: {
    color: theme.colors.surface,
  },
  presetsContainer: {
    paddingHorizontal: theme.spacing.xs,
  },
  presetCard: {
    width: 200,
    backgroundColor: theme.colors.surface,
    borderRadius: theme.borderRadius.m,
    padding: theme.spacing.m,
    marginHorizontal: theme.spacing.xs,
    borderWidth: 1,
    borderColor: theme.colors.border,
    ...theme.shadows.subtle,
  },
  presetCardSelected: {
    backgroundColor: theme.colors.green,
    borderColor: theme.colors.green,
    ...theme.shadows.medium,
  },
  presetThumbnail: {
    width: 60,
    height: 60,
    backgroundColor: theme.colors.lightGray,
    borderRadius: theme.borderRadius.m,
    justifyContent: 'center',
    alignItems: 'center',
    marginBottom: theme.spacing.m,
  },
  presetInfo: {
    flex: 1,
  },
  presetName: {
    fontSize: theme.fontSizes.headline,
    fontWeight: '600',
    color: theme.colors.text.primary,
    marginBottom: theme.spacing.xs,
  },
  presetNameSelected: {
    color: theme.colors.surface,
  },
  presetDescription: {
    fontSize: theme.fontSizes.footnote,
    color: theme.colors.text.secondary,
    lineHeight: 18,
  },
  presetDescriptionSelected: {
    color: theme.colors.surface,
    opacity: 0.9,
  },
  presetCheckmark: {
    position: 'absolute',
    top: theme.spacing.m,
    right: theme.spacing.m,
  },
  toggleRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    backgroundColor: theme.colors.surface,
    borderRadius: theme.borderRadius.m,
    padding: theme.spacing.m,
    borderWidth: 1,
    borderColor: theme.colors.border,
    ...theme.shadows.subtle,
  },
  toggleLeft: {
    flexDirection: 'row',
    alignItems: 'center',
    flex: 1,
  },
  toggleTextContainer: {
    marginLeft: theme.spacing.m,
    flex: 1,
  },
  toggleTitle: {
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.text.primary,
    marginBottom: theme.spacing.xs,
  },
  toggleSubtitle: {
    fontSize: theme.fontSizes.footnote,
    color: theme.colors.text.secondary,
  },
  collaboratorInputRow: {
    flexDirection: 'row',
    marginBottom: theme.spacing.m,
    gap: theme.spacing.s,
  },
  collaboratorInput: {
    flex: 1,
    backgroundColor: theme.colors.surface,
    borderRadius: theme.borderRadius.m,
    paddingHorizontal: theme.spacing.m,
    paddingVertical: theme.spacing.m,
    fontSize: theme.fontSizes.body,
    color: theme.colors.text.primary,
    borderWidth: 1,
    borderColor: theme.colors.border,
    ...theme.shadows.subtle,
  },
  addCollaboratorButton: {
    backgroundColor: theme.colors.green,
    borderRadius: theme.borderRadius.m,
    padding: theme.spacing.m,
    justifyContent: 'center',
    alignItems: 'center',
    ...theme.shadows.subtle,
  },
  collaboratorsContainer: {
    paddingHorizontal: theme.spacing.xs,
  },
  collaboratorChip: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: theme.colors.lightGray,
    borderRadius: theme.borderRadius.l,
    paddingHorizontal: theme.spacing.m,
    paddingVertical: theme.spacing.s,
    marginHorizontal: theme.spacing.xs,
    borderWidth: 1,
    borderColor: theme.colors.border,
  },
  collaboratorEmail: {
    fontSize: theme.fontSizes.footnote,
    color: theme.colors.text.primary,
    fontWeight: '500',
    marginRight: theme.spacing.xs,
  },
  removeCollaboratorButton: {
    padding: theme.spacing.xs,
  },
  createButton: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'center',
    backgroundColor: theme.colors.green,
    borderRadius: theme.borderRadius.m,
    paddingVertical: theme.spacing.l,
    paddingHorizontal: theme.spacing.xl,
    ...theme.shadows.medium,
  },
  createButtonText: {
    color: theme.colors.surface,
    fontSize: theme.fontSizes.headline,
    fontWeight: '600',
    marginLeft: theme.spacing.s,
  },
  bottomSpacing: {
    height: theme.spacing.xxl,
  },
  modalOverlay: {
    flex: 1,
    backgroundColor: 'rgba(0, 0, 0, 0.5)',
    justifyContent: 'flex-end',
  },
  datePickerModal: {
    backgroundColor: theme.colors.surface,
    borderTopLeftRadius: theme.borderRadius.xl,
    borderTopRightRadius: theme.borderRadius.xl,
    paddingBottom: theme.spacing.xl,
    ...theme.shadows.large,
  },
  datePickerHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingHorizontal: theme.spacing.l,
    paddingVertical: theme.spacing.m,
    borderBottomWidth: 1,
    borderBottomColor: theme.colors.border,
  },
  datePickerButton: {
    padding: theme.spacing.s,
  },
  datePickerButtonText: {
    fontSize: theme.fontSizes.body,
    color: theme.colors.text.secondary,
    fontWeight: '500',
  },
  datePickerButtonTextConfirm: {
    color: theme.colors.green,
    fontWeight: '600',
  },
  datePickerTitle: {
    fontSize: theme.fontSizes.headline,
    fontWeight: '600',
    color: theme.colors.text.primary,
  },
  datePickerContent: {
    padding: theme.spacing.l,
  },
  datePickerLabel: {
    fontSize: theme.fontSizes.body,
    color: theme.colors.text.secondary,
    marginBottom: theme.spacing.m,
    textAlign: 'center',
  },
  datePickerInput: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'center',
    backgroundColor: theme.colors.lightGray,
    borderRadius: theme.borderRadius.m,
    padding: theme.spacing.l,
    borderWidth: 1,
    borderColor: theme.colors.border,
  },
  datePickerInputText: {
    fontSize: theme.fontSizes.body,
    color: theme.colors.text.secondary,
    marginLeft: theme.spacing.s,
  },
});

export default AddProjectScreen;
