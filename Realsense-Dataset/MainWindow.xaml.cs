using RSDS;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.Windows.Threading;

namespace Realsense_Dataset
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        private RsDsController _rsds;
        private bool _running;

        public MainWindow()
        {
            InitializeComponent();
        }

        void AddOuput(string txt)
        {
            Dispatcher.BeginInvoke(DispatcherPriority.Input, new ThreadStart(() =>
            {
                TxtOutput.Text = TxtOutput.Text.Insert(0, txt + "\r\n");
            }));

            Thread.Sleep(0);
        }

        private void Window_Loaded(object sender, RoutedEventArgs e)
        {
            _rsds = new RsDsController();

            FolderName.Text = "testdata";

            _rsds.Initialize(StatusCallback);

        }

        protected virtual void StatusCallback(RsDsController.Status status, string description)
        {
            Debug.WriteLine($"StatusCallback: {status} - {description}");

            switch(status)
            {
                case RsDsController.Status.Initializing:
                    AddOuput("Initializing...");
                    CapturingEnabled(true);
                    break;
                case RsDsController.Status.Completed:
                    AddOuput("Initialized");
                    CapturingEnabled(false);
                    break;
                case RsDsController.Status.Started:
                    AddOuput("Started");
                    CapturingEnabled(false);
                    BtnStartStop.Content = "Stop";
                    ProcessingFrames(true);
                    break;
                case RsDsController.Status.Stopped:
                    AddOuput("Stopped");
                    CapturingEnabled(false);
                    BtnStartStop.Content = "Start";
                    break;
                case RsDsController.Status.Finalizing:
                    AddOuput("Finalizing...");
                    CapturingEnabled(true);
                    break;
                case RsDsController.Status.ErrorCameraUnplugged:
                    AddOuput("ERROR: Camera unplugged");
                    CapturingEnabled(false);
                    BtnStartStop.Content = "Start";
                    break;
                default:
                    if(!string.IsNullOrEmpty(description))
                        AddOuput(description);
                    break;
            }


        }

        private void BtnStartStop_Click(object sender, RoutedEventArgs e)
        {
            switch (_rsds.State)
            {
                case RsDsController.StateType.Initializing:
                    return;                
                case RsDsController.StateType.Started:
                    _rsds.Stop();
                    break;
                case RsDsController.StateType.Stopped:
                case RsDsController.StateType.Initialized:
                    if (string.IsNullOrWhiteSpace(FolderName.Text))
                        FolderName.Text = "testdata";
                    
                    _rsds.Start(FolderName.Text);
                    break;
            }
        }

        protected void ProcessingFrames(bool start)
        {
            if (_running == start)
                return;

            if (!start)
            {
                _running = false;
                return;
            }

            _running = true;

            var timer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(200) };
            timer.Tick += async (s, args) =>
            {
                timer.Stop();

                while (_running)
                {
                    await Task.Delay(1);

                    //var completion = new TaskCompletionSource<bool>();

                    //// do it in a thread so it doesn't tie up the ui
                    //await Task.Run(() =>
                    //{
                    //    _rsds.ProcessFrame();
                    //    completion.SetResult(true);
                    //});

                    //await completion.Task;

                    if (!_rsds.ProcessFrame())
                        continue;

                    _rsds.ProcessBitmapImage();

                    colorImage.Source = _rsds.GetColorBitmap();
                    depthImage.Source = _rsds.GetDepthBitmap();
                }

            };
            timer.Start();
        }

        protected void CapturingEnabled(bool enabled)
        {
            if(enabled)
            {
                BtnStartStop.IsEnabled = false;
                FolderName.IsEnabled = false;
                colorImage.Source = null;
                depthImage.Source = null;
            }
            else
            {
                BtnStartStop.IsEnabled = true;
                FolderName.IsEnabled = true;
                colorImage.Source = null;
                depthImage.Source = null;
            }
        }
    }
}
