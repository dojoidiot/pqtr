import SwiftUI

struct CreateProjectView: View {
    @EnvironmentObject var appViewModel: AppViewModel
    @Environment(\.dismiss) private var dismiss
    
    @State private var projectName = ""
    @State private var projectDescription = ""
    @State private var selectedOrganization: Organization?
    @State private var isCreating = false
    
    var body: some View {
        NavigationView {
            VStack(spacing: 20) {
                VStack(alignment: .leading, spacing: 8) {
                    Text("Project Name")
                        .font(.headline)
                    TextField("Enter project name", text: $projectName)
                        .textFieldStyle(RoundedBorderTextFieldStyle())
                }
                
                VStack(alignment: .leading, spacing: 8) {
                    Text("Description (Optional)")
                        .font(.headline)
                    TextField("Enter project description", text: $projectDescription, axis: .vertical)
                        .textFieldStyle(RoundedBorderTextFieldStyle())
                        .lineLimit(3...6)
                }
                
                VStack(alignment: .leading, spacing: 8) {
                    Text("Organization")
                        .font(.headline)
                    
                    if appViewModel.organizations.isEmpty {
                        Text("No organizations available")
                            .foregroundColor(.secondary)
                            .frame(maxWidth: .infinity, alignment: .leading)
                            .padding()
                            .background(Color.gray.opacity(0.1))
                            .cornerRadius(8)
                    } else {
                        Picker("Organization", selection: $selectedOrganization) {
                            Text("Select Organization").tag(nil as Organization?)
                            ForEach(appViewModel.organizations) { organization in
                                Text(organization.name).tag(organization as Organization?)
                            }
                        }
                        .pickerStyle(MenuPickerStyle())
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding()
                        .background(Color.gray.opacity(0.1))
                        .cornerRadius(8)
                    }
                }
                
                Spacer()
                
                Button("Create Project") {
                    createProject()
                }
                .disabled(!canCreateProject || isCreating)
                .frame(maxWidth: .infinity)
                .padding()
                .background(canCreateProject ? Color.blue : Color.gray)
                .foregroundColor(.white)
                .cornerRadius(10)
            }
            .padding()
            .navigationTitle("New Project")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .navigationBarLeading) {
                    Button("Cancel") {
                        dismiss()
                    }
                }
            }
        }
    }
    
    private var canCreateProject: Bool {
        !projectName.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty &&
        selectedOrganization != nil
    }
    
    private func createProject() {
        guard let organization = selectedOrganization else { return }
        
        isCreating = true
        
        Task {
            let trimmedDescription = projectDescription.trimmingCharacters(in: .whitespacesAndNewlines)
            let finalDescription = trimmedDescription.isEmpty ? nil : trimmedDescription
            await appViewModel.createProject(
                name: projectName.trimmingCharacters(in: .whitespacesAndNewlines),
                description: finalDescription,
                organizationId: organization.id
            )
            
            await MainActor.run {
                isCreating = false
                dismiss()
            }
        }
    }
}

#Preview {
    CreateProjectView()
        .environmentObject(AppViewModel())
}