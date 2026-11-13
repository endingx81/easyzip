using System.IO;
using System.Windows;
using System.Windows.Input;

namespace EasyZip.Views
{
    public partial class NewFolderDialog : Window
    {
        public string FolderName { get; private set; } = string.Empty;

        public NewFolderDialog(string defaultName = "새 폴더")
        {
            InitializeComponent();
            TxtFolderName.Text = defaultName;
            TxtFolderName.Focus();
            TxtFolderName.SelectAll();
        }

        private void BtnOk_Click(object sender, RoutedEventArgs e)
        {
            Confirm();
        }

        private void BtnCancel_Click(object sender, RoutedEventArgs e)
        {
            DialogResult = false;
            Close();
        }

        private void TxtFolderName_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.Enter)
            {
                Confirm();
            }
            else if (e.Key == Key.Escape)
            {
                DialogResult = false;
                Close();
            }
        }

        private void Confirm()
        {
            string name = TxtFolderName.Text.Trim();
            if (string.IsNullOrEmpty(name))
            {
                MessageBox.Show("폴더 이름을 입력해 주세요.", "알림", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            char[] invalidChars = Path.GetInvalidFileNameChars();
            if (name.IndexOfAny(invalidChars) >= 0)
            {
                MessageBox.Show("폴더 이름에 다음 문자는 사용할 수 없습니다:\n\\ / : * ? \" < > |", "알림", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            FolderName = name;
            DialogResult = true;
            Close();
        }
    }
}

