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
            

            AddOuput("Initialized");
        }

        protected virtual void StatusCallback(RsDsController.Status status, string description)
        {
            Debug.WriteLine($"StatusCallback: {status} - {description}");

            switch(status)
            {
                case RsDsController.Status.Initializing:
                    AddOuput("Starting up...");
                    BtnStartStop.IsEnabled = false;
                    break;
                case RsDsController.Status.Finalizing:
                    AddOuput("Stopping...");
                    BtnStartStop.IsEnabled = false;
                    break;
                case RsDsController.Status.RS_Started:
                    AddOuput("Started");
                    BtnStartStop.IsEnabled = true;
                    BtnStartStop.Content = "Stop";
                    ProcessingFrames(true);
                    break;
                case RsDsController.Status.RS_Stopped:
                    AddOuput("Stopped");
                    BtnStartStop.IsEnabled = true;
                    BtnStartStop.Content = "Start";
                    break;
                case RsDsController.Status.ErrorCameraUnplugged:
                    AddOuput("ERROR: Camera unplugged");
                    BtnStartStop.IsEnabled = true;
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
                case RsDsController.StateType.Stopping:
                    return;
                case RsDsController.StateType.Started:
                    ProcessingFrames(false);
                    _rsds.Stop();
                    break;
                case RsDsController.StateType.Stopped:
                    _rsds.Initialize("testdata", StatusCallback);
                    _rsds.Start();
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

                    _rsds.ProcessFrame();
                }

            };
            timer.Start();
        }
    }
}
