using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Windows.Media;
using EasyZip.Services;

namespace EasyZip.Models
{
    public class ArchiveFolderNode : INotifyPropertyChanged
    {
        private bool _isExpanded = false;
        private bool _isSelected = false;

        public string Name { get; set; } = string.Empty;
        public string FullPath { get; set; } = string.Empty;
        public ObservableCollection<ArchiveFolderNode> Children { get; set; } = new();
        public string Icon { get; set; } = "📁";
        public ImageSource FolderIcon => string.IsNullOrEmpty(FullPath) ? SystemIconHelper.GetFileIcon(Name) : SystemIconHelper.GetFolderIcon();

        public bool IsExpanded
        {
            get => _isExpanded;
            set
            {
                if (_isExpanded != value)
                {
                    _isExpanded = value;
                    OnPropertyChanged(nameof(IsExpanded));
                }
            }
        }

        public bool IsSelected
        {
            get => _isSelected;
            set
            {
                if (_isSelected != value)
                {
                    _isSelected = value;
                    OnPropertyChanged(nameof(IsSelected));
                }
            }
        }

        public event PropertyChangedEventHandler? PropertyChanged;
        protected void OnPropertyChanged(string propName) =>
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propName));
    }
}

