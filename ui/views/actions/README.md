# Integrating Actions and Views

Actions are essentially model objects. Following MVC principles, a view
controller should facilitate the interactions between actions and views.


ActionViewController is the main view controller to be instantiated or
subclassed. It should outlive all the views it manages. Call
ActioinViewController::CreateActionViewRelationship(..) to link a view to an
action item.


Many views will work out of the box with ActionViewController. However, we have
not completed supporting all basic view types. To allow ActionViewController to
support a new view class, simply implement an ActionViewInterface class for the
view class (See action_view_interface.h)/ ui/views/controls/button/button.* has
a concrete example.

### How to create an ActionViewInterface:

Note: Replace ViewType with the type of your View Class

#### Step 1: Create the ActionViewInterface

Instead of BaseActionViewInterface, subclass the ActionViewInterface subclass
associated with the parent view class to get action behaviors of the parent
class.

```
class ViewTypeActionViewInterface : public BaseActionViewInterface {
    public:
    explicit ViewTypeActionViewInterface(ViewType* action_view)
        : BaseActionViewInterface(action_view), action_view_(action_view) {}
    ~ViewTypeActionViewInterface() override = default;

    // optional: override virtual methods. See action_view_interface.h for
    // methods that can be overridden.

    private:
    raw_ptr<ViewType> action_view_;

};
```

#### Step 2: Override View::GetActionViewInterface

```
std::unique_ptr<ActionViewInterface> ViewType::GetActionViewInterface()
override {
   return std::make_unique<ViewTypeActionViewInterface>(this);
}
```
