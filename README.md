# Facebook extension

[Documentation](https://defold.github.io/extension-facebook)


# Relevant Facebook documentation links

* [Facebook Login](https://developers.facebook.com/docs/facebook-login)

* [Permissions](https://developers.facebook.com/docs/facebook-login/permissions)

* [Login Best Practices](https://developers.facebook.com/docs/facebook-login/best-practices)

# Changelog

## v2.0

* Added version function
	* facebook.get_version() -> string, e.g. `5.2.1`

* Added new login function
	* facebook.login_with_permissions()

* Removed deprecated functions
	* facebook.me()
	* facebook.login()
	* facebook.request_read_permissions()
	* facebook.request_publish_permissions()
	* facebook.login_with_read_permissions()
	* facebook.login_with_publish_permissions()

* Removed support for `appinvite` since it was removed in [FBSDK 4.28](https://developers.facebook.com/blog/post/2017/11/07/changes-developer-offerings/)

* The `"feed"` sharing dialog no longer supports these properties:
	* `title`
	* `description`
	* `picture`
