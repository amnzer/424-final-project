//This file has code taken from and influenced by the following sources:
//User raja_961, “Autonomous Lane-Keeping Car Using Raspberry
//Pi and OpenCV”. Instructables. URL:
//https://www.instructables.com/Autonomous-Lane-Keeping-Car-U
//sing-Raspberry-Pi-and
//Team ReaLly BaD Idea, "Autonomous path following car". URL:
//https://www.hackster.io/really-bad-idea/autonomous-path-following-car-6c4992
//Team Access Code, "Access Code Cybertruck'. URL: 
//https://www.hackster.io/497395/access-code-cybertruck-9f8b8c
//Adjustments had to be made to get our respective car to function correctly.


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/ktime.h>
#include <linux/platform_device.h>
#include <linux/of.h>

#define DRIVER_NAME "encoder_driver"
#define ENCODER_GPIO_LABEL "rotary_encoder"

// Structure to hold encoder device information
struct encoder_dev {
    struct gpio_desc *gpio;
    unsigned int irq_num;
    ktime_t last_event_time;
    s64 interval_ns;
    bool is_idle;
};

static struct encoder_dev my_encoder;

// Sysfs attribute show function to display the speed of the encoder
static ssize_t encoder_speed_show(struct device *dev, struct device_attribute *attr, char *buffer) {
    if (my_encoder.is_idle) {
        return sprintf(buffer, "0\n"); // Return 0 when encoder is idle
    }
    my_encoder.is_idle = true;
    return sprintf(buffer, "%lld\n", my_encoder.interval_ns); // Return the time interval of the last encoder event
}

static DEVICE_ATTR(speed, S_IRUGO, encoder_speed_show, NULL);

// Interrupt handler for the encoder GPIO
static irqreturn_t encoder_interrupt_handler(int irq, void *dev_id) {
    ktime_t current_time = ktime_get_real();
    ktime_t time_diff = ktime_sub(current_time, my_encoder.last_event_time);
    my_encoder.interval_ns = ktime_to_ns(time_diff);
    my_encoder.last_event_time = current_time;
    my_encoder.is_idle = false;

    // Log the encoder event with the interval time
    printk(KERN_INFO DRIVER_NAME ": Encoder event. Interval: %lld ns\n", my_encoder.interval_ns);
    return IRQ_HANDLED;
}

// Probe function called when the device is initialized
static int encoder_device_probe(struct platform_device *pdev) {
    printk(KERN_INFO DRIVER_NAME ": Initializing encoder device.\n");

    // Acquire the GPIO assigned to the encoder
    my_encoder.gpio = devm_gpiod_get(&pdev->dev, ENCODER_GPIO_LABEL, GPIOD_IN);
    if (IS_ERR(my_encoder.gpio)) {
        printk(KERN_ALERT DRIVER_NAME ": Failed to acquire GPIO.\n");
        return PTR_ERR(my_encoder.gpio);
    }

    // Request an IRQ for the GPIO
    my_encoder.irq_num = gpiod_to_irq(my_encoder.gpio);
    if (request_irq(my_encoder.irq_num, encoder_interrupt_handler, IRQF_TRIGGER_FALLING, DRIVER_NAME, NULL)) {
        printk(KERN_ALERT DRIVER_NAME ": IRQ request failed.\n");
        return -EIO;
    }

    // Initialize the encoder's last event time and set it to idle
    my_encoder.last_event_time = ktime_get_real();
    my_encoder.is_idle = true;
    sysfs_create_file(&pdev->dev.kobj, &dev_attr_speed.attr); // Create a sysfs file for speed

    printk(KERN_INFO DRIVER_NAME ": Device initialized successfully.\n");
    return 0;
}

// Remove function called when the device is removed
static int encoder_device_remove(struct platform_device *pdev) {
    free_irq(my_encoder.irq_num, NULL); // Free the IRQ
    sysfs_remove_file(&pdev->dev.kobj, &dev_attr_speed.attr); // Remove the sysfs file
    printk(KERN_INFO DRIVER_NAME ": Device removed.\n");
    return 0;
}

// Match table for device tree compatibility
static const struct of_device_id encoder_of_match[] = {
    { .compatible = "custom,rotary-encoder" },
    {},
};

MODULE_DEVICE_TABLE(of, encoder_of_match);

// Platform driver structure
static struct platform_driver encoder_platform_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = encoder_of_match,
    },
    .probe = encoder_device_probe,
    .remove = encoder_device_remove,
};

// Register the platform driver
module_platform_driver(encoder_platform_driver);

MODULE_AUTHOR("Modified Author");
MODULE_DESCRIPTION("Custom Rotary Encoder Driver");
MODULE_LICENSE("GPL");
